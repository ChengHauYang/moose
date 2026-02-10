//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "SideUserObject.h"
#include "MooseTypes.h"

#include <map>

/**
 * NEML2SideBatchIndexGenerator iterates over boundary sides and generates a map from
 * (element ID, side, boundary ID) to batch index which is used by NEML2ModelExecutor for transfer
 * data between MOOSE and NEML2 on sides.
 */
class NEML2SideBatchIndexGenerator : public SideUserObject
{
public:
  static InputParameters validParams();

  NEML2SideBatchIndexGenerator(const InputParameters & params);

  void initialize() override;
  void execute() override;
  void threadJoin(const UserObject &) override;
  void finalize() override;

  void meshChanged() override;

  struct SideKey
  {
    dof_id_type elem_id = libMesh::invalid_uint;
    unsigned int side = libMesh::invalid_uint;
    BoundaryID boundary = Moose::INVALID_BOUNDARY_ID;

    bool operator<(const SideKey & other) const
    {
      if (elem_id != other.elem_id)
        return elem_id < other.elem_id;
      if (side != other.side)
        return side < other.side;
      return boundary < other.boundary;
    }
  };

  /// Get the current batch index (in almost all cases this is the total batch size)
  std::size_t getBatchIndex() const { return _batch_index; }

  /// Get the batch index for the given element/side/boundary tuple
  std::size_t getBatchIndex(dof_id_type elem_id, unsigned int side, BoundaryID boundary) const;

  /// Access the side-to-batch-index map (local sides only)
  const std::map<SideKey, std::size_t> & sideToBatchIndex() const { return _side_to_batch_index; }

  /// Access the side-to-nqp map (local sides only)
  const std::map<SideKey, unsigned int> & sideToNQPs() const { return _side_to_nqp; }

  /// Whether the batch is empty
  bool isEmpty() const { return _batch_index == 0; }

protected:
  /// Whether the batch index map is outdated
  bool _outdated;

  /// Highest current batch index
  std::size_t _batch_index;

  /// Map from side keys to batch indices
  std::map<SideKey, std::size_t> _side_to_batch_index;

  /// Map from side keys to number of quadrature points
  std::map<SideKey, unsigned int> _side_to_nqp;

  /// cache the index for the current side
  mutable std::pair<SideKey, std::size_t> _side_to_batch_index_cache;
};
