//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BreakMeshByBlockGenerator.h"
#include "CastUniquePointer.h"
#include "MooseMeshUtils.h"
#include "libmesh/distributed_mesh.h"
#include "libmesh/elem.h"
#include "libmesh/partitioner.h"
#include "timpi/parallel_sync.h"
#include <typeinfo>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

registerMooseObject("MooseApp", BreakMeshByBlockGenerator);

InputParameters
BreakMeshByBlockGenerator::validParams()
{
  InputParameters params = BreakMeshByBlockGeneratorBase::validParams();
  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addClassDescription(
      "Break the mesh at interfaces between blocks. New nodes will be generated so elements on "
      "each side of the break are no longer connected.");
  params.addParam<std::vector<SubdomainName>>(
      "surrounding_blocks",
      "The list of subdomain names surrounding which interfaces will be generated.");
  params.addParam<std::vector<std::vector<SubdomainName>>>(
      "block_pairs", "The list of subdomain pairs between which interfaces will be generated.");
  params.addParam<bool>("add_transition_interface",
                        false,
                        "If true and block is not empty, a special boundary named "
                        "interface_transition is generate between listed blocks and other blocks.");
  params.addParam<bool>(
      "split_transition_interface", false, "Whether to split the transition interface by blocks.");
  params.addParam<bool>("add_interface_on_two_sides",
                        false,
                        "Whether to add an additional interface boundary at the other side.");
  params.addParam<BoundaryName>(
      "interface_transition_name",
      "interface_transition",
      "the name of the interface transition boundary created when blocks are provided");
  params.addParam<bool>(
      "debug", false, "Emit detailed debug logs to the console to aid development.");
  return params;
}

BreakMeshByBlockGenerator::BreakMeshByBlockGenerator(const InputParameters & parameters)
  : BreakMeshByBlockGeneratorBase(parameters),
    _input(getMesh("input")),
    _block_pairs_restricted(parameters.isParamSetByUser("block_pairs")),
    _surrounding_blocks_restricted(parameters.isParamSetByUser("surrounding_blocks")),
    _add_transition_interface(getParam<bool>("add_transition_interface")),
    _split_transition_interface(getParam<bool>("split_transition_interface")),
    _interface_transition_name(getParam<BoundaryName>("interface_transition_name")),
    _add_interface_on_two_sides(getParam<bool>("add_interface_on_two_sides")),
    _debug(getParam<bool>("debug"))
{
  if (_block_pairs_restricted && _surrounding_blocks_restricted)
    paramError("block_pairs_restricted",
               "BreakMeshByBlockGenerator: 'surrounding_blocks' and 'block_pairs' can not be used "
               "at the same time.");
  if (!_add_transition_interface && _split_transition_interface)
    paramError("split_transition_interface",
               "BreakMeshByBlockGenerator cannot split the transition interface because "
               "add_transition_interface is false");
  if (_add_transition_interface && _block_pairs_restricted)
    paramError("add_transition_interface",
               "BreakMeshByBlockGenerator cannot split the transition interface when 'block_pairs' "
               "are specified.");
}

std::unique_ptr<MeshBase>
BreakMeshByBlockGenerator::generate()
{
  std::unique_ptr<MeshBase> mesh = std::move(_input);
  mesh->prepare_for_use();
  BoundaryInfo & boundary_info = mesh->get_boundary_info();

  const auto max_id = mesh->parallel_max_unique_id();

  if (_debug)
    _console << "[BreakMeshByBlockGenerator] begin on rank " << mesh->processor_id()
             << ", replicated = " << std::boolalpha << mesh->is_replicated() << std::noboolalpha
             << std::endl;

  // Handle block restrictions
  if (_block_pairs_restricted)
  {
    for (const auto & block_name_pair :
         getParam<std::vector<std::vector<SubdomainName>>>("block_pairs"))
    {
      if (block_name_pair.size() != 2)
        paramError("block_pairs",
                   "Each row of 'block_pairs' must have a size of two (block names).");
      for (const auto & name : block_name_pair)
        if (!MooseMeshUtils::hasSubdomainName(*mesh, name))
          paramError("block_pairs", "The block '", name, "' was not found in the mesh");
      const auto block_pair = MooseMeshUtils::getSubdomainIDs(*mesh, block_name_pair);
      std::pair<SubdomainID, SubdomainID> pair = std::make_pair(
          std::min(block_pair[0], block_pair[1]), std::max(block_pair[0], block_pair[1]));
      _block_pairs.insert(pair);
      std::copy(block_pair.begin(), block_pair.end(), std::inserter(_block_set, _block_set.end()));
    }
  }
  else if (_surrounding_blocks_restricted)
  {
    for (const auto & name : getParam<std::vector<SubdomainName>>("surrounding_blocks"))
      if (!MooseMeshUtils::hasSubdomainName(*mesh, name))
        paramError("surrounding_blocks", "The block '", name, "' was not found in the mesh");
    const auto surrounding_block_ids = MooseMeshUtils::getSubdomainIDs(
        *mesh, getParam<std::vector<SubdomainName>>("surrounding_blocks"));
    std::copy(surrounding_block_ids.begin(),
              surrounding_block_ids.end(),
              std::inserter(_block_set, _block_set.end()));
  }

  if ((_block_pairs_restricted || _surrounding_blocks_restricted) && _add_transition_interface &&
      boundary_info.get_id_by_name(_interface_transition_name) != Moose::INVALID_BOUNDARY_ID)
    paramError(
        "interface_transition_name",
        "BreakMeshByBlockGenerator the specified interface transition boundary name already exits");

  // Build local node-to-element map
  std::map<dof_id_type, std::vector<dof_id_type>> node_to_elem_map;
  for (const auto & elem : mesh->active_element_ptr_range())
    for (unsigned int n = 0; n < elem->n_nodes(); n++)
      node_to_elem_map[elem->node_id(n)].push_back(elem->id());

  // Sync connected block info across all ranks
  syncConnectedBlocks(node_to_elem_map, *mesh);
  if (_debug)
    _console << "[BreakMeshByBlockGenerator] rank " << mesh->processor_id()
             << " finished syncing connected blocks map" << std::endl;

  // Globally sync and create a deterministic map for new node IDs
  syncAndMapNewNodeIds(node_to_elem_map, *mesh);
  if (_debug)
    _console << "[BreakMeshByBlockGenerator] rank " << mesh->processor_id()
             << " finished creating new node ID map" << std::endl;

  // Main loop to duplicate nodes
  for (const auto & map_entry : node_to_elem_map)
  {
    const dof_id_type current_node_id = map_entry.first;
    const Node * current_node = mesh->node_ptr(current_node_id);
    if (!current_node)
      continue;

    const auto & connected_blocks = _nodeid_to_connected_blocks.at(current_node_id);
    const unsigned int node_multiplicity = connected_blocks.size();

    //
    // if (mesh->processor_id() != current_node->processor_id())
    //   continue;

    if (node_multiplicity > 1)
    {
      const std::vector<dof_id_type> & connected_elems = map_entry.second;
      subdomain_id_type reference_subdomain_id = connected_blocks.count(Elem::invalid_subdomain_id)
                                                     ? Elem::invalid_subdomain_id
                                                     : *connected_blocks.begin();

      bool should_create_new_node = true;
      if (_block_pairs_restricted)
      {
        if (connected_blocks.size() == 2)
        {
          auto it = connected_blocks.begin();
          should_create_new_node = findBlockPairs(*it, *std::next(it, 1));
        }
        else
          should_create_new_node = false;
      }

      unsigned int multiplicity_counter = node_multiplicity;
      for (auto elem_id : connected_elems)
      {
        if (multiplicity_counter == 0)
          break;

        Elem * current_elem = mesh->elem_ptr(elem_id);
        if (!current_elem)
          continue;

        //
        if (mesh->processor_id() != current_elem->processor_id())
          continue;
        if (current_node->processor_id() != current_elem->processor_id())
          continue;
        if (mesh->processor_id() != current_node->processor_id())
          continue;

        subdomain_id_type block_id = blockRestrictedElementSubdomainID(current_elem);

        if ((block_id != reference_subdomain_id) ||
            (_block_pairs_restricted && findBlockPairs(reference_subdomain_id, block_id)))
        {
          Node * new_node = nullptr;
          std::vector<boundary_id_type> node_boundary_ids;

          for (unsigned int n_idx = 0; n_idx < current_elem->n_nodes(); ++n_idx)
          {
            if (current_elem->node_id(n_idx) == current_node->id())
            {
              if (should_create_new_node)
              {
                new_node =
                    Node::build(
                        *current_node,
                        mesh->is_replicated()
                            ? mesh->n_nodes()
                            : /*(current_elem->subdomain_id() + 1) * max_id + current_node
                                                                                ->unique_id()*/
                            computeGlobalNewNodeId(current_node_id, elem_id))
                        .release();
                new_node->processor_id() = current_elem->processor_id();
                mesh->add_node(new_node);
                // new_node = mesh->add_point(*current_node,
                //                            mesh->is_replicated() ? mesh->n_nodes() :
                //                                                  /*(current_elem->subdomain_id()
                //                                                  +
                //                                                     1) * max_id +
                //                                                        current_node->unique_id()*/
                //                                computeGlobalNewNodeId(current_node_id, elem_id));
                current_elem->set_node(n_idx, new_node);

                boundary_info.boundary_ids(current_node, node_boundary_ids);
                boundary_info.add_node(new_node, node_boundary_ids);
              }
              multiplicity_counter--;
              break;
            }
          }

          if (should_create_new_node && new_node)
          {
            for (auto connected_elem_id : connected_elems)
            {
              Elem * connected_elem = mesh->elem_ptr(connected_elem_id);
              if (connected_elem &&
                  connected_elem->subdomain_id() == current_elem->subdomain_id() &&
                  connected_elem != current_elem)
              {
                for (unsigned int n_idx = 0; n_idx < connected_elem->n_nodes(); ++n_idx)
                  if (connected_elem->node_id(n_idx) == current_node->id())
                  {
                    connected_elem->set_node(n_idx, new_node);
                    break;
                  }
              }
            }
          }
        }
      }

      // Record sides for new interface boundaries
      for (auto elem_id : connected_elems)
        for (auto connected_elem_id : connected_elems)
        {
          Elem * current_elem = mesh->elem_ptr(elem_id);
          Elem * connected_elem = mesh->elem_ptr(connected_elem_id);

          if (!current_elem || !connected_elem)
            continue;

          subdomain_id_type curr_elem_subid = blockRestrictedElementSubdomainID(current_elem);
          subdomain_id_type connected_elem_subid =
              blockRestrictedElementSubdomainID(connected_elem);

          if (current_elem != connected_elem && curr_elem_subid < connected_elem_subid &&
              current_elem->has_neighbor(connected_elem))
          {
            dof_id_type id1 = current_elem->id();
            unsigned int side1 = current_elem->which_neighbor_am_i(connected_elem);
            dof_id_type id2 = connected_elem->id();
            unsigned int side2 = connected_elem->which_neighbor_am_i(current_elem);

            std::pair<subdomain_id_type, subdomain_id_type> pair1 = {curr_elem_subid,
                                                                     connected_elem_subid};
            std::pair<subdomain_id_type, subdomain_id_type> pair2 = {connected_elem_subid,
                                                                     curr_elem_subid};

            if (!_block_pairs_restricted || findBlockPairs(curr_elem_subid, connected_elem_subid))
            {
              _new_boundary_sides_map[pair1].insert({id1, side1});
              if (_add_interface_on_two_sides)
                _new_boundary_sides_map[pair2].insert({id2, side2});
            }
          }
        }
    }
  }

  // addInterfaceBoundary(*mesh);
  // Partitioner::set_node_processor_ids(*mesh);
  if (_debug)
    _console << "[BreakMeshByBlockGenerator] done on rank " << mesh->processor_id() << std::endl;
  return dynamic_pointer_cast<MeshBase>(mesh);
}

void
BreakMeshByBlockGenerator::addInterfaceBoundary(MeshBase & mesh)
{
  BoundaryInfo & boundary_info = mesh.get_boundary_info();

  boundary_id_type boundary_id;
  boundary_id_type boundary_id_interface = Moose::INVALID_BOUNDARY_ID;
  boundary_id_type boundary_id_interface_transition = Moose::INVALID_BOUNDARY_ID;

  BoundaryName boundary_name;

  // loop over boundary sides
  for (auto & boundary_side_map : _new_boundary_sides_map)
  {
    if (!(_block_pairs_restricted || _surrounding_blocks_restricted) ||
        ((_block_pairs_restricted || _surrounding_blocks_restricted) && !_add_transition_interface))
    {
      // find the appropriate boundary name and id
      // given primary and secondary block ID
      if (_split_interface)
        findBoundaryNameAndInd(mesh,
                               boundary_side_map.first.first,
                               boundary_side_map.first.second,
                               boundary_name,
                               boundary_id,
                               boundary_info);
      else
      {
        boundary_name = _interface_name;
        boundary_id_interface = boundary_id_interface == Moose::INVALID_BOUNDARY_ID
                                    ? findFreeBoundaryId(mesh)
                                    : boundary_id_interface;
        boundary_id = boundary_id_interface;
        boundary_info.sideset_name(boundary_id_interface) = boundary_name;
      }
    }
    else // block resticted with transition boundary
    {

      const bool interior_boundary =
          _block_set.find(boundary_side_map.first.first) != _block_set.end() &&
          _block_set.find(boundary_side_map.first.second) != _block_set.end();

      if ((_split_interface && interior_boundary) ||
          (_split_transition_interface && !interior_boundary))
      {
        findBoundaryNameAndInd(mesh,
                               boundary_side_map.first.first,
                               boundary_side_map.first.second,
                               boundary_name,
                               boundary_id,
                               boundary_info);
      }
      else if (interior_boundary)
      {
        boundary_name = _interface_name;
        boundary_id_interface = boundary_id_interface == Moose::INVALID_BOUNDARY_ID
                                    ? findFreeBoundaryId(mesh)
                                    : boundary_id_interface;

        boundary_id = boundary_id_interface;
        boundary_info.sideset_name(boundary_id_interface) = boundary_name;
      }
      else
      {
        boundary_id_interface_transition =
            boundary_id_interface_transition == Moose::INVALID_BOUNDARY_ID
                ? findFreeBoundaryId(mesh)
                : boundary_id_interface_transition;
        boundary_id = boundary_id_interface_transition;
        boundary_info.sideset_name(boundary_id) = _interface_transition_name;
      }
    }
    // loop over all the side belonging to each pair and add it to the proper interface
    for (auto & element_side : boundary_side_map.second)
      boundary_info.add_side(element_side.first, element_side.second, boundary_id);
  }
}

subdomain_id_type
BreakMeshByBlockGenerator::blockRestrictedElementSubdomainID(const Elem * elem) const
{
  subdomain_id_type elem_subdomain_id = elem->subdomain_id();
  if ((_block_pairs_restricted || _surrounding_blocks_restricted) &&
      (_block_set.find(elem_subdomain_id) == _block_set.end()))
    elem_subdomain_id = Elem::invalid_subdomain_id;
  return elem_subdomain_id;
}

bool
BreakMeshByBlockGenerator::findBlockPairs(subdomain_id_type block_one, subdomain_id_type block_two)
{
  for (auto block_pair : _block_pairs)
    if ((block_pair.first == block_one && block_pair.second == block_two) ||
        (block_pair.first == block_two && block_pair.second == block_one))
      return true;
  return false;
}

void
BreakMeshByBlockGenerator::syncConnectedBlocks(
    const std::map<dof_id_type, std::vector<dof_id_type>> & node_to_elem_map, MeshBase & mesh)
{
  // Phase 0: Each rank computes its local connected blocks for nodes it holds
  for (const auto & map_entry : node_to_elem_map)
  {
    const auto & elem_ids = map_entry.second;
    for (const dof_id_type elem_id : elem_ids)
    {
      const Elem * elem = mesh.elem_ptr(elem_id);
      if (elem)
      {
        const subdomain_id_type bid = blockRestrictedElementSubdomainID(elem);
        if (!_block_pairs_restricted || bid != Elem::invalid_subdomain_id)
          _nodeid_to_connected_blocks[map_entry.first].insert(bid);
      }
    }
  }

  if (mesh.is_replicated())
    return; // No synchronization needed for replicated meshes

  auto & comm = mesh.comm();
  const auto mesh_pid = mesh.processor_id();

  // Phase 1: Ghost nodes push their connected blocks to the owner
  using NodeConnectedBlocksTuple =
      std::tuple<dof_id_type, std::vector<subdomain_id_type>, processor_id_type>;
  std::map<processor_id_type, std::vector<NodeConnectedBlocksTuple>> to_owner;
  for (const auto & map_entry : _nodeid_to_connected_blocks)
  {
    const auto node_id = map_entry.first;
    const Node * node = mesh.node_ptr(node_id);
    if (node && node->processor_id() != mesh_pid)
    {
      const auto & blocks = map_entry.second;
      to_owner[node->processor_id()].emplace_back(
          node_id, std::vector<subdomain_id_type>(blocks.begin(), blocks.end()), mesh_pid);
    }
  }

  // Owner receives and merges ghost info into its local map, and tracks subscribers
  std::unordered_map<dof_id_type, std::set<processor_id_type>> subscribers;
  Parallel::push_parallel_vector_data(
      comm,
      to_owner,
      [&](processor_id_type, const std::vector<NodeConnectedBlocksTuple> & recv_data)
      {
        for (const auto & [node_id, blocks_vec, ghost_pid] : recv_data)
        {
          subscribers[node_id].insert(ghost_pid);
          _nodeid_to_connected_blocks[node_id].insert(blocks_vec.begin(), blocks_vec.end());
        }
      });

  // Phase 2: Node owners broadcast complete connected blocks back to subscribers
  using NodeConnectedBlocksPair = std::pair<dof_id_type, std::vector<subdomain_id_type>>;
  std::map<processor_id_type, std::vector<NodeConnectedBlocksPair>> from_owner;
  for (const auto & [node_id, sub_pids] : subscribers)
  {
    const Node * node = mesh.node_ptr(node_id);
    if (node && node->processor_id() == mesh_pid)
    {
      const auto & blocks_set = _nodeid_to_connected_blocks.at(node_id);
      for (const auto pid : sub_pids)
        from_owner[pid].emplace_back(
            node_id, std::vector<subdomain_id_type>(blocks_set.begin(), blocks_set.end()));
    }
  }

  Parallel::push_parallel_vector_data(
      comm,
      from_owner,
      [&](processor_id_type, const std::vector<NodeConnectedBlocksPair> & recv_data)
      {
        for (const auto & [node_id, blocks_vec] : recv_data)
          _nodeid_to_connected_blocks[node_id].insert(blocks_vec.begin(), blocks_vec.end());
      });
}

void
BreakMeshByBlockGenerator::syncAndMapNewNodeIds(
    const std::map<dof_id_type, std::vector<dof_id_type>> & node_to_elem_map, MeshBase & mesh)
{

  const auto max_id = mesh.parallel_max_unique_id();
  if (mesh.is_replicated())
    return; // Not needed for replicated meshes. IDs will be assigned sequentially.

  std::set<std::pair<dof_id_type, dof_id_type>> local_pairs_to_split;

  // 1. Each rank determines its LOCAL (node, elem) pairs that need splitting based on complete
  // block info.
  for (const auto & map_entry : node_to_elem_map)
  {
    const dof_id_type current_node_id = map_entry.first;
    const auto & connected_blocks = _nodeid_to_connected_blocks.at(current_node_id);

    if (connected_blocks.size() <= 1)
      continue;

    bool should_create_new_node = true;
    if (_block_pairs_restricted)
    {
      if (connected_blocks.size() == 2)
      {
        auto it1 = connected_blocks.begin();
        auto it2 = std::next(it1);
        should_create_new_node = findBlockPairs(*it1, *it2);
      }
      else
        should_create_new_node = false;
    }

    if (!should_create_new_node)
      continue;

    subdomain_id_type reference_subdomain_id = connected_blocks.count(Elem::invalid_subdomain_id)
                                                   ? Elem::invalid_subdomain_id
                                                   : *connected_blocks.begin();

    for (const auto elem_id : map_entry.second)
    {
      const Elem * elem = mesh.elem_ptr(elem_id);
      if (!elem)
        continue;

      subdomain_id_type block_id = blockRestrictedElementSubdomainID(elem);
      if ((block_id != reference_subdomain_id) ||
          (_block_pairs_restricted && findBlockPairs(reference_subdomain_id, block_id)))
      {
        local_pairs_to_split.insert({current_node_id, elem_id});
      }
    }
  }

  // 2. Gather all local lists into one global list on all ranks.
  std::vector<std::pair<dof_id_type, dof_id_type>> global_pairs_vec(local_pairs_to_split.begin(),
                                                                    local_pairs_to_split.end());
  mesh.comm().allgather(global_pairs_vec);

  // 3. Sort the global list to ensure deterministic ID assignment across all ranks.
  std::sort(global_pairs_vec.begin(), global_pairs_vec.end());

  // 4. Create the global ID map. Every rank does this identically.
  unique_id_type next_id = max_id + 1;
  for (const auto & pair : global_pairs_vec)
  {
    // To avoid duplication, only insert if the key is not present.
    // This handles cases where different ranks might report the same pair if elements are shared.
    if (_new_node_elem_pairs_to_new_node_id.find(pair) == _new_node_elem_pairs_to_new_node_id.end())
    {
      _new_node_elem_pairs_to_new_node_id[pair] = next_id;
      next_id++;
    }
  }
}

dof_id_type
BreakMeshByBlockGenerator::computeGlobalNewNodeId(dof_id_type node_id, dof_id_type elem_id) const
{
  auto it = _new_node_elem_pairs_to_new_node_id.find({node_id, elem_id});
  if (it != _new_node_elem_pairs_to_new_node_id.end())
    return it->second;
  else
    mooseError("[computeGlobalNewNodeId] Could not find new node ID for (node_id=" +
               std::to_string(node_id) + ", elem_id=" + std::to_string(elem_id) + ")");
}
