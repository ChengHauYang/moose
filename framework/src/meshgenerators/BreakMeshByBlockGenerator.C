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
#include <typeinfo>

registerMooseObject("MooseApp", BreakMeshByBlockGenerator);
#include <set>
#include <vector>
#include <map>
#include <algorithm>

#include "timpi/parallel_sync.h"

// Debug utilities removed; use the 'debug' parameter with _console instead.

InputParameters
BreakMeshByBlockGenerator::validParams()
{
  InputParameters params = BreakMeshByBlockGeneratorBase::validParams();
  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addClassDescription(
      "Break the mesh at interfaces between blocks. New nodes will be generated so elements on "
      "each side of the break are no longer connected. At the moment, this only works on a "
      "REPLICATED mesh");
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
      "use_n_nodes",
      false,
      "If true, use the number of nodes in the mesh to set the number of new nodes.");
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
    _use_n_nodes(getParam<bool>("use_n_nodes")),
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
  // Prepare mesh for operations
  mesh->prepare_for_use();

  BoundaryInfo & boundary_info = mesh->get_boundary_info();

  const unique_id_type max_unique_id = mesh->parallel_max_unique_id();

  if (_debug)
  {
    _console << "[BreakMeshByBlockGenerator] begin on rank " << mesh->processor_id()
             << ", replicated = " << std::boolalpha << mesh->is_replicated() << std::noboolalpha
             << std::endl;
    _console << "  options: block_pairs_restricted=" << _block_pairs_restricted
             << ", surrounding_blocks_restricted=" << _surrounding_blocks_restricted
             << ", add_transition_interface=" << _add_transition_interface
             << ", split_transition_interface=" << _split_transition_interface
             << ", use_n_nodes=" << _use_n_nodes << std::endl;
  }

  // Handle block restrictions
  if (_block_pairs_restricted)
  {
    for (const auto & block_name_pair :
         getParam<std::vector<std::vector<SubdomainName>>>("block_pairs"))
    {
      if (block_name_pair.size() != 2)
        paramError("block_pairs",
                   "Each row of 'block_pairs' must have a size of two (block names).");

      // check that the blocks exist in the mesh
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
    // check that the blocks exist in the mesh
    for (const auto & name : getParam<std::vector<SubdomainName>>("surrounding_blocks"))
      if (!MooseMeshUtils::hasSubdomainName(*mesh, name))
        paramError("surrounding_blocks", "The block '", name, "' was not found in the mesh");

    const auto surrounding_block_ids = MooseMeshUtils::getSubdomainIDs(
        *mesh, getParam<std::vector<SubdomainName>>("surrounding_blocks"));
    std::copy(surrounding_block_ids.begin(),
              surrounding_block_ids.end(),
              std::inserter(_block_set, _block_set.end()));
  }

  // check that a boundary named _interface_transition_name does not already exist in the mesh
  if ((_block_pairs_restricted || _surrounding_blocks_restricted) && _add_transition_interface &&
      boundary_info.get_id_by_name(_interface_transition_name) != Moose::INVALID_BOUNDARY_ID)
    paramError("interface_transition_name",
               "BreakMeshByBlockGenerator the specified  interface transition boundary name "
               "already exits");

  // initialize the node to element map
  std::map<dof_id_type, std::vector<dof_id_type>> node_to_elem_map;
  for (const auto & elem : mesh->active_element_ptr_range())
    for (unsigned int n = 0; n < elem->n_nodes(); n++)
      node_to_elem_map[elem->node_id(n)].push_back(elem->id());

  auto prepare_connected_blocks = [&](const std::vector<dof_id_type> & elem_ids,
                                      std::set<subdomain_id_type> & connected_blocks_set)
  {
    for (auto elem_id_local = elem_ids.begin(); elem_id_local != elem_ids.end(); elem_id_local++)
    {
      const Elem * current_elem_local = mesh->elem_ptr(*elem_id_local);

      subdomain_id_type block_id_local = blockRestrictedElementSubdomainID(current_elem_local);
      if (!_block_pairs_restricted)
        connected_blocks_set.insert(block_id_local);
      else
      {
        if (block_id_local != Elem::invalid_subdomain_id)
          connected_blocks_set.insert(block_id_local);
      }
    }
  };

  // (1) Non-owner ranks send the connected blocks of ghost nodes to the owner rank.
  // (2) The owner rank receives this data and supplements it with its own local elements' connected
  // blocks.
  // (3) The owner rank then broadcasts the complete set of connected blocks to other ranks
  // that need it.
  for (auto node_it = node_to_elem_map.begin(); node_it != node_to_elem_map.end(); ++node_it)
  {
    const dof_id_type current_node_id = node_it->first;
    const Node * current_node = mesh->node_ptr(current_node_id);

    if (!current_node)
      continue;

    prepare_connected_blocks(node_it->second, _nodeid_to_connected_blocks[current_node_id]);
    if (!mesh->is_replicated())
    {
      // std::unordered_map<processor_id_type, std::vector<dof_id_type>> push_data;
      std::unordered_map<processor_id_type, std::vector<NodeToConnectedBlocksMap>>
          push_data_nodeid_and_connected_blocks;

      // (1) Push ghost node and its connected blocks to the owner processor
      if (current_node->processor_id() != mesh->processor_id())
      {
        NodeToConnectedBlocksMap single_entry_map;
        auto it = _nodeid_to_connected_blocks.find(current_node_id);
        if (it != _nodeid_to_connected_blocks.end())
          single_entry_map.insert({it->first, it->second});

        push_data_nodeid_and_connected_blocks[current_node->processor_id()].push_back(
            single_entry_map);
      }

      // (2) Owner processor prepare connected blocks
      auto push_receiver =
          [&](const processor_id_type, const std::vector<NodeToConnectedBlocksMap> & received_data)
      {
        for (const auto & single_entry_map : received_data)
        {
          dof_id_type node_id = 0;
          for (const auto & kv : single_entry_map)
          {
            _nodeid_to_connected_blocks[kv.first].insert(kv.second.begin(), kv.second.end());
            node_id = kv.first;
          }

          const Node * current_node = mesh->node_ptr(node_id);
          if (!current_node)
            continue;
          prepare_connected_blocks(node_to_elem_map[node_id], _nodeid_to_connected_blocks[node_id]);
        }
      };
      Parallel::push_parallel_vector_data(
          _mesh->comm(), push_data_nodeid_and_connected_blocks, push_receiver);

      // (3) Owner processor push back connected blocks to other processors
      push_data_nodeid_and_connected_blocks.clear();
      // std::unordered_map<processor_id_type, std::vector<NodeToConnectedBlocksMap>>
      //     push_data_nodeid_and_connected_blocks;

      if (current_node->processor_id() == mesh->processor_id())
      {
        NodeToConnectedBlocksMap single_entry_map;
        auto it = _nodeid_to_connected_blocks.find(current_node_id);
        if (it != _nodeid_to_connected_blocks.end())
          single_entry_map.insert({it->first, it->second});

        for (processor_id_type pid = 0; pid < n_processors(); ++pid)
          if (pid != processor_id())
            push_data_nodeid_and_connected_blocks[pid].push_back(single_entry_map);
      }

      auto push_receiver_nodeid_and_connected_blocks =
          [&](const processor_id_type, const std::vector<NodeToConnectedBlocksMap> & received_data)
      {
        for (const auto & single_entry_map : received_data)
          for (const auto & kv : single_entry_map)
            _nodeid_to_connected_blocks[kv.first].insert(kv.second.begin(), kv.second.end());
      };

      Parallel::push_parallel_vector_data(_mesh->comm(),
                                          push_data_nodeid_and_connected_blocks,
                                          push_receiver_nodeid_and_connected_blocks);
    }
  }

  // std::map<std::pair<dof_id_type, dof_id_type>, dof_id_type> new_nodes_order_in_current_proc;
  // dof_id_type local_new_nodes = 0;
  // for (auto node_it = node_to_elem_map.begin(); node_it != node_to_elem_map.end(); ++node_it)
  // {
  //   const auto current_node_id = node_it->first;
  //   for (auto elem_id : node_it->second)
  //   {
  //     const Elem * current_elem = mesh->elem_ptr(elem_id);
  //     subdomain_id_type block_id = blockRestrictedElementSubdomainID(current_elem);
  //     if (!_block_pairs_restricted || block_id != Elem::invalid_subdomain_id)
  //     {
  //       new_nodes_order_in_current_proc[std::make_pair(current_node_id, elem_id)] =
  //       local_new_nodes; local_new_nodes++;
  //     }
  //   }
  // }

  // if (_debug)
  // {
  //   auto & out = _console;
  //   out << "[BreakMeshByBlockGenerator] rank " << mesh->processor_id()
  //       << " planned new_nodes=" << local_new_nodes << std::endl;
  // }

  // dof_id_type proc_first_new_id = 0;

  // if (!mesh->is_replicated())
  // {
  //   // Distributed: use prefix sum to allocate non-overlapping id ranges
  //   std::vector<unsigned int> counts(mesh->comm().size());
  //   mesh->comm().allgather(static_cast<unsigned int>(local_new_nodes), counts);

  //   unsigned int offset = 0;
  //   for (unsigned int p = 0; p < mesh->processor_id(); ++p)
  //     offset += counts[p];

  //   dof_id_type max_id = mesh->max_node_id();
  //   mesh->comm().max(max_id);

  //   const auto global_start_id = max_id + 1;
  //   proc_first_new_id = global_start_id + offset;

  //   if (_debug)
  //     _console << "[BreakMeshByBlockGenerator] rank " << mesh->processor_id()
  //              << " local_new_nodes=" << local_new_nodes << ", offset=" << offset
  //              << ", proc_first_new_id=" << proc_first_new_id << std::endl;
  // }

  typedef std::pair<dof_id_type, dof_id_type> NodeElemIDPair;
  for (auto node_it = node_to_elem_map.begin(); node_it != node_to_elem_map.end(); ++node_it)
  {
    const dof_id_type current_node_id = node_it->first;
    const Node * current_node = mesh->node_ptr(current_node_id);

    if (!current_node)
      continue;

    // find node multiplicity
    const auto connected_blocks = _nodeid_to_connected_blocks[node_it->first];

    unsigned int node_multiplicity = connected_blocks.size();

    // check if current_node need to be duplicated
    if (node_multiplicity > 1)
    {
      // retrieve connected elements from the map
      const std::vector<dof_id_type> & connected_elems = node_it->second;

      // find reference_subdomain_id (e.g. the subdomain with lower id or the
      // Elem::invalid_subdomain_id)
      auto subdomain_it = connected_blocks.begin();
      subdomain_id_type reference_subdomain_id =
          connected_blocks.find(Elem::invalid_subdomain_id) != connected_blocks.end()
              ? Elem::invalid_subdomain_id
              : *subdomain_it;

      // For the block_pairs option, the node that is only shared by specific block pairs will
      // be newly created.
      bool should_create_new_node = true;
      if (_block_pairs_restricted)
      {
        auto elems = node_to_elem_map[current_node->id()];
        should_create_new_node = false;
        std::set<subdomain_id_type> sets_blocks_for_this_node;
        for (auto elem_id = elems.begin(); elem_id != elems.end(); elem_id++)
          sets_blocks_for_this_node.insert(
              blockRestrictedElementSubdomainID(mesh->elem_ptr(*elem_id)));
        if (sets_blocks_for_this_node.size() == 2)
        {
          auto setIt = sets_blocks_for_this_node.begin();
          if (findBlockPairs(*setIt, *std::next(setIt, 1)))
            should_create_new_node = true;
        }
      }

      // multiplicity counter to keep track of how many nodes we added
      unsigned int multiplicity_counter = node_multiplicity;
      bool push_node_to_other_procs = false;
      for (auto elem_id : connected_elems)
      {
        // all the duplicate nodes are added and assigned
        if (multiplicity_counter == 0)
          break;

        Elem * current_elem = mesh->elem_ptr(elem_id);

        subdomain_id_type block_id = blockRestrictedElementSubdomainID(current_elem);

        if ((blockRestrictedElementSubdomainID(current_elem) != reference_subdomain_id) ||
            (_block_pairs_restricted & findBlockPairs(reference_subdomain_id, block_id)))
        {
          // assign the newly added node to current_elem
          Node * new_node = nullptr;

          std::vector<boundary_id_type> node_boundary_ids;

          for (unsigned int node_id = 0; node_id < current_elem->n_nodes(); ++node_id)
            if (current_elem->node_id(node_id) ==
                current_node->id()) // if current node == node on element
            {
              if (should_create_new_node)
              {
                // const auto new_nodes_order_number_in_current_proc =
                //     new_nodes_order_in_current_proc[std::make_pair(current_node_id, elem_id)];

                // new_node =
                //     Node::build(*current_node,
                //                 mesh->is_replicated()
                //                     ? mesh->n_nodes()
                //                     : proc_first_new_id + new_nodes_order_number_in_current_proc)
                //         .release(); // or: new_node_id = (connected_subdomain_id+1) * max_node_id
                //         +
                //                     // old_node_id
                new_node = Node::build(*current_node,
                                       mesh->is_replicated()
                                           ? mesh->n_nodes()
                                           : (current_elem->subdomain_id() + 1) * max_unique_id +
                                                 current_node->unique_id())
                               .release(); // or: new_node_id = (connected_subdomain_id+1) *
                                           // max_node_id + old_node_id

                // We're duplicating nodes so that each subdomain elem has its own copy, so it
                // seems natural to assign this new node the same proc id as corresponding
                // subdomain elem
                new_node->processor_id() = current_elem->processor_id();
                mesh->add_node(new_node);
                current_elem->set_node(node_id,
                                       new_node); // override the original node with
                                                  // the new one, remember that node_id
                                                  // is local to the current element
                // Add boundary info to the new node
                // First figure out the old node belong to which boundary ids
                // For example, if node is belong to bottom (ID = 3) and left (ID = 5)
                // boundary, then node_boundary_ids = {3, 5}
                boundary_info.boundary_ids(current_node, node_boundary_ids);
                // Then add the new node to the same boundary ids
                // Add new_node to the nodeset (node boundary) data structure, ensuring no
                // duplicates, validity, and bidirectional lookup.
                boundary_info.add_node(new_node, node_boundary_ids);
                // }
              }

              multiplicity_counter--; // node created, update multiplicity counter

              break; // ones the proper node has been fixed in one element we can break the
                     // loop
            }

          // now we need to assign the newly added node to "other connected elements" with the
          // "same block_id"
          if (should_create_new_node)
          {
            for (auto connected_elem_id : connected_elems)
            {
              Elem * connected_elem = mesh->elem_ptr(connected_elem_id);

              // Assign the newly added node to other connected elements with the same
              // block_id
              if (connected_elem->subdomain_id() == current_elem->subdomain_id() &&
                  connected_elem != current_elem)
              {
                for (unsigned int node_id = 0; node_id < connected_elem->n_nodes(); ++node_id)
                  if (connected_elem->node_id(node_id) ==
                      current_node->id()) // if current node == node on element
                  {
                    connected_elem->set_node(node_id, new_node);
                    break;
                  }
              }
            }
          }
        }
      }

      // Create block pairs and assign element sides to the new interface boundary map.
      // Purpose: Identify sides between elements that were just split at nodes and are truly
      // adjacent but belong to different blocks. These sides are "recorded" in
      // "_new_boundary_sides_map", which will later be used to generate new interface
      // boundaries.
      for (auto elem_id :
           connected_elems) // connected_elems: all elements connected to the same old node
                            // (elements from different blocks are included).
      {
        for (auto connected_elem_id : connected_elems)
        {
          Elem * current_elem = mesh->elem_ptr(elem_id);
          Elem * connected_elem = mesh->elem_ptr(connected_elem_id);
          subdomain_id_type curr_elem_subid = blockRestrictedElementSubdomainID(current_elem);
          subdomain_id_type connected_elem_subid =
              blockRestrictedElementSubdomainID(connected_elem);

          if (current_elem != connected_elem && curr_elem_subid < connected_elem_subid)
          {
            if (current_elem->has_neighbor(
                    connected_elem)) // Only consider elements that are "actually adjacent" in
                                     // the mesh (share an edge/face)
            {
              dof_id_type elem_id = current_elem->id();
              dof_id_type connected_elem_id = connected_elem->id();
              unsigned int side = current_elem->which_neighbor_am_i(connected_elem);
              unsigned int connected_elem_side = connected_elem->which_neighbor_am_i(current_elem);

              // in this case we need to play a game to reorder the sides
              // When block_pairs or surrounding_blocks restrictions are used, only real block
              // IDs (not invalid_subdomain_id) should be put into the pair. Here,
              // (curr_elem_subid, connected_elem_subid) are reordered to ensure first <
              // second and both are valid block IDs. Also, subid, elem_id and side are
              // swapped accordingly to keep the mapping direction consistent.
              if (_block_pairs_restricted || _surrounding_blocks_restricted)
              {
                connected_elem_subid = connected_elem->subdomain_id();
                if (curr_elem_subid > connected_elem_subid) // we need to switch the ids
                {
                  connected_elem_subid = current_elem->subdomain_id();
                  curr_elem_subid = connected_elem->subdomain_id();

                  elem_id = connected_elem->id();
                  side = connected_elem->which_neighbor_am_i(current_elem);

                  connected_elem_id = current_elem->id();
                  connected_elem_side = current_elem->which_neighbor_am_i(connected_elem);
                }
              }

              // blocks_pair: forward (low ID, high ID).
              // blocks_pair2: reverse. If `_add_interface_on_two_sides` is enabled, the
              // element/side on the reverse pair will also be stored, ensuring both sides are
              // marked as boundary.
              std::pair<subdomain_id_type, subdomain_id_type> blocks_pair =
                  std::make_pair(curr_elem_subid, connected_elem_subid);

              std::pair<subdomain_id_type, subdomain_id_type> blocks_pair2 =
                  std::make_pair(connected_elem_subid, curr_elem_subid);

              // _new_boundary_sides_map is a map from block pairs to a set of element sides,
              // i.e., (block1, block2) -> { (elem_id, side_id), ... }
              // That is: which element sides are located between block1 and block2.
              // The addInterfaceBoundary() function will later convert this entire map into
              // actual BoundaryInfo.
              // Example: _new_boundary_sides_map = {
              //    (3,7) : { {12,1}, {15,2}, {20,0}, ... },   // all sides between block 3
              //    and block 7 (7,3) : { {25,3}, ... },                  // if double-sided,
              //    reverse direction (4,invalid) : { ... }                     // interface
              //    between restricted and unrestricted blocks
              // }

              auto add_boundary_sides =
                  [&](const std::pair<subdomain_id_type, subdomain_id_type> & pair1,
                      const std::pair<subdomain_id_type, subdomain_id_type> & pair2,
                      dof_id_type id1,
                      unsigned int side1,
                      dof_id_type id2,
                      unsigned int side2)
              {
                _new_boundary_sides_map[pair1].insert(std::make_pair(id1, side1));
                if (_add_interface_on_two_sides)
                  _new_boundary_sides_map[pair2].insert(std::make_pair(id2, side2));
              };

              if (_block_pairs_restricted)
              {
                if (findBlockPairs(blockRestrictedElementSubdomainID(current_elem),
                                   blockRestrictedElementSubdomainID(connected_elem)))
                {
                  add_boundary_sides(blocks_pair,
                                     blocks_pair2,
                                     elem_id,
                                     side,
                                     connected_elem_id,
                                     connected_elem_side);
                }
              }
              else
              {
                add_boundary_sides(blocks_pair,
                                   blocks_pair2,
                                   elem_id,
                                   side,
                                   connected_elem_id,
                                   connected_elem_side);
              }
            }
          }
        }
      }

    } // end multiplicity check
  } // end loop over nodes

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

  // loop over boundary sides
  for (auto & boundary_side_map : _new_boundary_sides_map)
  {
    // Decide which boundary name / id to use for this blocks_pair
    if (!(_block_pairs_restricted || _surrounding_blocks_restricted) ||
        // No block restrictions, or restrictions exist but transition interface is NOT
        // requested
        ((_block_pairs_restricted || _surrounding_blocks_restricted) && !_add_transition_interface))
    {
      // --- Standard case --------------------------------------------------
      if (_split_interface) // Split each block pair into a separate interface name
        findBoundaryNameAndInd(mesh,
                               boundary_side_map.first.first,  // block A
                               boundary_side_map.first.second, // block B
                               boundary_name,
                               boundary_id,
                               boundary_info);
      else // All block pairs share the same interface name
      {
        boundary_name = _interface_name; // Usually "interface"
        // If not yet assigned, find a free boundary id
        boundary_id_interface = boundary_id_interface == Moose::INVALID_BOUNDARY_ID
                                    ? findFreeBoundaryId(mesh)
                                    : boundary_id_interface;
        boundary_id = boundary_id_interface;
        boundary_info.sideset_name(boundary_id_interface) = boundary_name;
      }
    }
    else // --- Block restrictions exist and transition interface is requested
         // ------------------
    {
      // Both blocks in _block_set -> interior; only one in set -> exterior (transition)
      const bool interior_boundary = _block_set.count(boundary_side_map.first.first) &&
                                     _block_set.count(boundary_side_map.first.second);

      if ((_split_interface && interior_boundary) ||
          (_split_transition_interface && !interior_boundary))
      {
        // Assign a unique name for this block pair (could be interior or transition)
        findBoundaryNameAndInd(mesh,
                               boundary_side_map.first.first,
                               boundary_side_map.first.second,
                               boundary_name,
                               boundary_id,
                               boundary_info);
      }
      else if (interior_boundary) // Interior, not splitting: use _interface_name for all
      {
        boundary_name = _interface_name;
        boundary_id_interface = boundary_id_interface == Moose::INVALID_BOUNDARY_ID
                                    ? findFreeBoundaryId(mesh)
                                    : boundary_id_interface;

        boundary_id = boundary_id_interface;
        boundary_info.sideset_name(boundary_id_interface) = boundary_name;
      }
      else // Exterior (transition): use transition interface name/id
      {
        boundary_id_interface_transition =
            boundary_id_interface_transition == Moose::INVALID_BOUNDARY_ID
                ? findFreeBoundaryId(mesh)
                : boundary_id_interface_transition;
        boundary_id = boundary_id_interface_transition;
        boundary_info.sideset_name(boundary_id) =
            _interface_transition_name; // Default is "interface_transition"
      }
    }

    // loop over all the side belonging to each pair and add it to the proper interface
    for (auto & element_side : boundary_side_map.second /*(element ID, side ID)*/)
      boundary_info.add_side(
          element_side.first /*element ID*/, element_side.second /*side ID*/, boundary_id);
  }
}

// Return rules:
//   (1) If no block restrictions are active, just give back elem->subdomain_id().
//   (2) If restrictions are active:
//       - If the element’s block is in the allowed set, return its real subdomain_id.
//       - Otherwise, return Elem::invalid_subdomain_id.
//
// Effect: All "unlisted" blocks collapse into one dummy block so we don’t
//         duplicate nodes or build interfaces in regions we’re not interested in.
subdomain_id_type
BreakMeshByBlockGenerator::blockRestrictedElementSubdomainID(const Elem * elem)
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
  // Using set<pair> to store _block_pairs ensures first < second, so lookup works regardless of
  // order.
  for (auto block_pair : _block_pairs)
    if ((block_pair.first == block_one && block_pair.second == block_two) ||
        (block_pair.first == block_two && block_pair.second == block_one))
      return true;
  return false;
}
