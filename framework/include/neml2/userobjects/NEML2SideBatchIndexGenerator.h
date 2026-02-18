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
#include <tuple>

/**
 * NEML2SideBatchIndexGenerator iterates over boundary sides and generates a map from
 * (element ID, side) to batch index which is used by NEML2ModelExecutor for transfer
 * data between MOOSE and NEML2 on sides.
 *
 *
 * This class is closely related to `NEML2BatchIndexGenerator`. The main difference is that this
 * class generates batch indices for sides instead of elements. The batch index is generated for
 * each (element ID, side) tuple and stored in a map. The batch index is used by NEML2ModelExecutor
 * to transfer data between MOOSE and NEML2 on sides.
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

  using ElemSide = std::tuple<dof_id_type, unsigned int>;

  /// Get the current batch index (in almost all cases this is the total batch size)
  std::size_t getBatchIndex() const { return _batch_index; }

  /// Get the batch index for the given element/side tuple
  std::size_t getBatchIndex(const ElemSide & elem_side) const;

  /// Access the side-to-batch-index map (local sides only)
  const std::map<ElemSide, std::size_t> & sideToBatchIndex() const { return _side_to_batch_index; }

  /// Whether the batch is empty
  bool isEmpty() const { return _batch_index == 0; }

protected:
  /// Whether the batch index map is outdated
  bool _outdated;

  /// Highest current batch index
  std::size_t _batch_index;

  /// Map from side keys to batch indices
  std::map<ElemSide, std::size_t> _side_to_batch_index;

  /// cache the index for the current side
  mutable std::pair<ElemSide, std::size_t> _side_to_batch_index_cache;
};
