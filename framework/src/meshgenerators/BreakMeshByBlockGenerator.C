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
        // if (mesh->processor_id() != current_elem->processor_id())
        //   continue;
        // if (current_node->processor_id() != current_elem->processor_id())
        //   continue;
        // if (mesh->processor_id() != current_node->processor_id())
        //   continue;

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
                    Node::
                        build(
                            *current_node,
                            mesh->is_replicated()
                                ? mesh->n_nodes()
                                : (current_elem->subdomain_id() + 1) * max_id + current_node
                                                                                    ->unique_id()
                                                                                    /*computeGlobalNewNodeId(current_node_id,
                                                                                    elem_id)*/)
                            .release();
                new_node->processor_id() = current_elem->processor_id();
                mesh->add_node(new_node);
                // new_node = mesh->add_point(*current_node,
                //                            mesh->is_replicated() ? mesh->n_nodes()
                //                                                  :
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

  addInterfaceBoundary(*mesh);
  Partitioner::set_node_processor_ids(*mesh);
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

  for (auto & boundary_side_map : _new_boundary_sides_map)
  {
    if (!(_block_pairs_restricted || _surrounding_blocks_restricted) ||
        ((_block_pairs_restricted || _surrounding_blocks_restricted) && !_add_transition_interface))
    {
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
    else
    {
      const bool interior_boundary = _block_set.count(boundary_side_map.first.first) &&
                                     _block_set.count(boundary_side_map.first.second);
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

// void
// BreakMeshByBlockGenerator::syncConnectedBlocks(
//     const std::map<dof_id_type, std::vector<dof_id_type>> & node_to_elem_map, MeshBase & mesh)
// {
//   const auto mesh_pid = mesh.processor_id();

//   auto & comm = mesh.comm();

//   // ---- Phase 0: each rank computes its local connected blocks ----
//   for (auto it = node_to_elem_map.begin(); it != node_to_elem_map.end(); ++it)
//   {
//     auto node_id = it->first;
//     auto node = mesh.node_ptr(node_id);
//     if (!node)
//       continue;

//     const auto node_owner_pid = node->processor_id();

//     for (auto elem_id : it->second)
//     {
//       auto * elem = mesh.elem_ptr(elem_id);
//       if (!elem)
//         continue;

//       if (node_owner_pid == mesh_pid)
//       {
//         // if (_subdomain_local_new_node_id.find(std::make_pair(node_id, elem->id())) ==
//         //     _subdomain_local_new_node_id.end())
//         _node_elem_pairs.insert(std::make_pair(node_id, elem->id()));
//         // {
//         //   // Node owner also takes care of setting up new node indexing
//         //   _new_node_each_subdomain[blockRestrictedElementSubdomainID(elem) - 1]++;
//         //   _subdomain_local_new_node_id[std::make_pair(node_id, elem->id())] =
//         //       _new_node_each_subdomain[blockRestrictedElementSubdomainID(elem) - 1];
//         // }
//       }
//     }

//     prepareConnectedBlocks(it->second, _nodeid_to_connected_blocks[node_id], mesh);
//   }

//   if (mesh.is_replicated())
//     return; // no synchronization needed for replicated meshes

//   // ---- Phase 1: ghost nodes push their connected blocks to the owner ----
//   using NodeConnectedBlocksTuple =
//       std::tuple<dof_id_type, std::vector<subdomain_id_type>, processor_id_type>;
//   std::map<processor_id_type, std::vector<NodeConnectedBlocksTuple>>
//       to_owner; // owner_pid -> [(node_id, blocks_vec, ghost_pid), ...]

//   for (auto it = _nodeid_to_connected_blocks.begin(); it != _nodeid_to_connected_blocks.end();
//   ++it)
//   {
//     auto node_id = it->first;
//     auto node = mesh.node_ptr(node_id);
//     if (!node)
//       continue;

//     const auto node_owner_pid = node->processor_id();

//     if (node_owner_pid != mesh_pid) // ghost node should send block info to the owner
//     {
//       const auto & blocks = it->second;
//       to_owner[node_owner_pid].push_back(
//           NodeConnectedBlocksTuple(node_id,
//                                    std::vector<subdomain_id_type>(blocks.begin(), blocks.end()),
//                                    mesh_pid /*ghosted pid*/));
//     }
//   }

//   // owner receives and merges ghost info into its local map
//   // Subscribers: node_id -> set of ghost_pids
//   std::unordered_map<dof_id_type, std::set<processor_id_type>> subscribers;

//   Parallel::push_parallel_vector_data(
//       comm,
//       to_owner,
//       [&](processor_id_type /*src*/, const std::vector<NodeConnectedBlocksTuple> & recv_data)
//       {
//         for (const auto & [node_id, blocks_vec, ghost_pid] : recv_data)
//         {
//           subscribers[node_id].insert(ghost_pid);
//           _nodeid_to_connected_blocks[node_id].insert(blocks_vec.begin(), blocks_vec.end());

//           auto itNE = node_to_elem_map.find(node_id);
//           if (itNE != node_to_elem_map.end())
//           {
//             for (auto elem_id : itNE->second)
//             {
//               auto * elem = mesh.elem_ptr(elem_id);
//               if (!elem)
//                 continue;

//               _node_elem_pairs.insert(std::make_pair(node_id, elem->id()));

//               // if (_subdomain_local_new_node_id.find(std::make_pair(node_id, elem->id())) ==
//               //     _subdomain_local_new_node_id.end())
//               // {
//               // Node owner also takes care of setting up new node indexing
//               // _new_node_each_subdomain[blockRestrictedElementSubdomainID(elem) - 1]++;
//               // _subdomain_local_new_node_id[std::make_pair(node_id, elem->id())] =
//               //     _new_node_each_subdomain[blockRestrictedElementSubdomainID(elem) - 1];
//               // }
//             }

//             prepareConnectedBlocks(itNE->second, _nodeid_to_connected_blocks[node_id], mesh);
//           }
//         }
//       });

//   to_owner.clear();

//   // ---- Phase 2: node owners broadcast complete connected blocks to all processors ----
//   using NodeConnectedBlocksPair = std::pair<dof_id_type, std::vector<subdomain_id_type>>;
//   std::map<processor_id_type, std::vector<NodeConnectedBlocksPair>> from_owner;

//   // Iterate over all subscribers recorded in Phase 1.
//   // Each entry corresponds to one node_id and the set of ghost ranks
//   // that previously reported block connectivity for that node.
//   for (const auto & sub_entry : subscribers)
//   {
//     const auto node_id = sub_entry.first;
//     const auto & subscriber_pids = sub_entry.second; // set of ghost processor IDs

//     // Safety: skip if subscriber list looks suspiciously large
//     if (subscriber_pids.size() > mesh.n_processors() - 1)
//     {
//       if (_debug)
//         _console << "[syncConnectedBlocks] WARNING: node " << node_id << " has "
//                  << subscriber_pids.size() << " subscribers (possible bug)" << std::endl;
//       continue;
//     }

//     // Only the owner of the node is allowed to perform the broadcast.
//     // Other ranks simply skip this node.
//     const auto * node = mesh.node_ptr(node_id);
//     if (!node || node->processor_id() != mesh_pid)
//       continue;

//     // Retrieve the full set of connected blocks for this node.
//     // At this point the owner has already merged information from ghosts.
//     const auto map_it = _nodeid_to_connected_blocks.find(node_id);
//     if (map_it == _nodeid_to_connected_blocks.end())
//       continue; // Should not happen, but check for safety.

//     const auto & blocks_set = map_it->second;

//     // Send this payload to all ghost ranks that subscribed to this node.
//     for (const auto pid : subscriber_pids)
//     {
//       if (pid == mesh_pid) // Do not send back to the owner itself
//         continue;
//       from_owner[pid].push_back(NodeConnectedBlocksPair(
//           node_id, std::vector<subdomain_id_type>(blocks_set.begin(), blocks_set.end())));
//     }
//   }

//   // for (auto it = _nodeid_to_connected_blocks.begin(); it != _nodeid_to_connected_blocks.end();
//   // ++it)
//   // {
//   //   auto node_id = it->first;
//   //   auto node = mesh.node_ptr(node_id);
//   //   if (!node)
//   //     continue;

//   //   auto node_owner_pid = node->processor_id();
//   //   if (node_owner_pid != mesh_pid)
//   //     continue; // only the owner sends

//   //   auto itSubs = subscribers.find(node_id);
//   //   if (itSubs == subscribers.end())
//   //     continue; // no subscribers for this node

//   //   const auto & blocks = it->second;
//   //   NodeConnectedBlocksPair payload(node_id,
//   //                                   std::vector<subdomain_id_type>(blocks.begin(),
//   //                                   blocks.end()));

//   //   for (processor_id_type pid = 0; pid < n_processors(); ++pid)
//   //     if (pid != processor_id())
//   //       from_owner[pid].push_back(payload);
//   // }

//   Parallel::push_parallel_vector_data(
//       comm,
//       from_owner,
//       [&](processor_id_type /*src*/, const std::vector<NodeConnectedBlocksPair> & recv_data)
//       {
//         for (const auto & [node_id, blocks_vec] : recv_data)
//           _nodeid_to_connected_blocks[node_id].insert(blocks_vec.begin(), blocks_vec.end());
//       });

//   // Change _node_elem_pairs to vector for easier communication
//   _node_elem_pairs_vector.assign(_node_elem_pairs.begin(), _node_elem_pairs.end());

//   comm.allgather(_node_elem_pairs_vector);

//   const unique_id_type max_unique_id = mesh.parallel_max_unique_id();
//   int i = 0;
//   for (const auto & pair : _node_elem_pairs_vector)
//   {
//     _new_node_elem_pairs_to_new_node_id[pair] = max_unique_id + i;
//     i++;
//   }

//   // // Debug print after allgather
//   if (_debug)
//   {
//     for (const auto & pair : _node_elem_pairs_vector)
//       _console << "rank " << mesh.processor_id() << "  node_id = " << pair.first
//                << ", elem_id = " << pair.second << std::endl;
//   }

//   from_owner.clear();
//   subscribers.clear();
// }

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
        auto it = connected_blocks.begin();
        should_create_new_node = findBlockPairs(*it, *std::next(it, 1));
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
  std::vector<std::pair<dof_id_type, dof_id_type>> local_pairs_vec(local_pairs_to_split.begin(),
                                                                   local_pairs_to_split.end());
  mesh.comm().allgather(local_pairs_vec);

  // 3. Sort the global list to ensure deterministic ID assignment across all ranks.
  std::sort(local_pairs_vec.begin(), local_pairs_vec.end());

  // 4. Create the global ID map. Every rank does this identically.
  unique_id_type next_id = max_id + 1;
  for (const auto & pair : local_pairs_vec)
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
