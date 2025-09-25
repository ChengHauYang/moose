//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "BreakMeshByBlockGeneratorBase.h"
#include <unordered_set>
#include "FakeNeighborRM.h"

/*
 * A mesh generator to split a mesh by a set of blocks
 */
class BreakMeshByBlockGenerator : public BreakMeshByBlockGeneratorBase
{
public:
  static InputParameters validParams();

  BreakMeshByBlockGenerator(const InputParameters & parameters);

  std::unique_ptr<MeshBase> generate() override;

protected:
  /// This is a helper method to avoid recoding the same if everywhere.
  /// If this mesh modifier is used in block restricted mode and the provided
  /// element belongs to one of the provided blocks it returns the
  /// element subdomain id, an invalid subdomain id otherwise.
  /// If this mesh modifier is not block restricted, then the method always
  /// returns the element subdomain id.
  /// Notice that in block restricted mode, the invalid_subdomain_id is used
  /// to lump toghether all the non-listed blocks to avoid splitting the mesh
  /// where not necessary.
  subdomain_id_type blockRestrictedElementSubdomainID(const Elem * elem) const;

  /// Return true if block_one and block_two are found in users' provided block_pairs list
  bool findBlockPairs(subdomain_id_type block_one, subdomain_id_type block_two);

  /// the mesh to modify
  std::unique_ptr<MeshBase> & _input;
  /// set of subdomain pairs between which interfaces will be generated.
  std::unordered_set<std::pair<SubdomainID, SubdomainID>> _block_pairs;
  /// set of the blocks to split the mesh on
  std::unordered_set<SubdomainID> _block_set;
  /// whether interfaces will be generated between block pairs
  const bool _block_pairs_restricted;
  /// whether interfaces will be generated surrounding blocks
  const bool _surrounding_blocks_restricted;
  /// whether to add a boundary when splitting the mesh
  const bool _add_transition_interface;
  /// whether to split the transition boundary between the blocks and the rest of the mesh
  const bool _split_transition_interface;
  /// the name of the transition interface
  const BoundaryName _interface_transition_name;
  /// whether to add two sides interface boundaries
  const bool _add_interface_on_two_sides;
  /// whether to generate boundary pairs between blocks
  const bool _generate_boundary_pairs;

private:
  /// generate the new boundary interface
  void addInterfaceBoundary(MeshBase & mesh);

  std::set<std::pair<subdomain_id_type, subdomain_id_type>> _neighboring_block_list;

  std::set<std::pair<subdomain_id_type, subdomain_id_type>> _new_boundary_sides_list;
  /// @brief a map from a pair of block ids to a set of element and side pairs
  std::map<std::pair<subdomain_id_type, subdomain_id_type>,
           std::set<std::pair<const Elem *, unsigned int>>>
      _new_boundary_sides_map;

  typedef std::pair<dof_id_type, unsigned int> ElemSidePair;
  typedef std::pair<ElemSidePair, ElemSidePair> PairOfElemSidePair;

  std::unordered_map<std::pair<const Elem *, unsigned int>, std::pair<const Elem *, unsigned int>>
      _elem_side_to_fake_neighbor_elem_side;
  std::unordered_map<ElemSidePair, ElemSidePair> _elemid_side_to_fake_neighbor_elemid_side;

  bool _prepare_end;

  // Typedef for mapping node_id -> set of connected block_ids
  typedef std::map<dof_id_type, std::set<subdomain_id_type>> NodeToConnectedBlocksMap;
  NodeToConnectedBlocksMap _nodeid_to_connected_blocks;

  // Typedef for a single message entry: (node_id, vector of connected block_ids)
  typedef std::pair<dof_id_type, std::vector<subdomain_id_type>> NodeConnectedBlocksPair;

  // Typedef for communication map: processor_id -> list of (node_id, block_ids)
  typedef std::unordered_map<processor_id_type, std::vector<NodeConnectedBlocksPair>>
      NodeConnectedBlocksCommMap;

  std::map<std::pair<dof_id_type, dof_id_type>, dof_id_type> _new_node_elem_pairs_to_new_node_id;

  void prepare_connected_blocks(const std::vector<dof_id_type> & elem_ids,
                                std::set<subdomain_id_type> & connected_blocks_set,
                                MeshBase & mesh);
  void syncConnectedBlocks(const std::map<dof_id_type, std::vector<dof_id_type>> & node_to_elem_map,
                           MeshBase & mesh);
  void
  syncAndMapNewNodeIds(const std::map<dof_id_type, std::vector<dof_id_type>> & node_to_elem_map,
                       MeshBase & mesh);
  dof_id_type computeGlobalNewNodeId(dof_id_type node_id, dof_id_type elem_id) const;
};
