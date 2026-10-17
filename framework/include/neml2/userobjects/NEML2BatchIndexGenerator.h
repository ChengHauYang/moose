//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "DomainUserObject.h"

#include <map>

/**
 * NEML2BatchIndexGenerator iterates over the mesh and generates a map from element ID to batch
 * index which is used by NEML2ModelExecutor for transfer data between MOOSE and NEML2.
 */
class NEML2BatchIndexGenerator : public DomainUserObject
{
public:
  static InputParameters validParams();

  NEML2BatchIndexGenerator(const InputParameters & params);

  void initialize() override;
  void executeOnElement() override;
  void executeOnBoundary() override;
  void executeOnInterface() override;
  void threadJoin(const UserObject &) override;
  void finalize() override;

  void meshChanged() override;

  using ElemSide = std::pair<dof_id_type, unsigned int>;
  using StateBatchIndexMap = std::map<ElemSide, std::size_t>;

  /// Get the current batch index (in almost all cases this is the total batch size)
  std::size_t getBatchIndex() const { return _batch_index; }

  /// Get the batch index for the given element ID
  std::size_t getBatchIndex(dof_id_type elem_id) const;

  /// Get the element-to-batch-index map
  const std::map<dof_id_type, std::size_t> & getBatchIndexMap() const;

  /// Get the element-side-to-batch-index map
  const StateBatchIndexMap & getSideBatchIndexMap() const { return _elemside_to_batch_index; }

  /// Get the state ownership map for the active batch layout
  StateBatchIndexMap getStateBatchIndexMap() const;

  /// Get the batch index for the given element side
  std::size_t getSideBatchIndex(const ElemSide & elem_side) const;

  /// Check if a batch index exists for the given element side
  bool isSideBatchIndexExist(const ElemSide & elem_side) const;

  /// Whether the batch is empty
  bool isEmpty() const { return _batch_index == 0; }

  /// Whether the element-to-batch-index map must be regenerated
  bool isOutdated() const { return _outdated; }

protected:
  /// When true, skip volume-element indexing and assign batch indices for interface side QPs only.
  const bool _interface_only;

  /// Whether the batch index map is outdated
  bool _outdated;

  /// Highest current batch index
  std::size_t _batch_index;

  /// Map from element IDs to batch indices
  std::map<dof_id_type, std::size_t> _elem_to_batch_index;

  /// Map from element sides to batch indices
  StateBatchIndexMap _elemside_to_batch_index;

  /// cache the index for the current element
  mutable std::pair<dof_id_type, std::size_t> _elem_to_batch_index_cache;

  /// cache the index for the current element side
  mutable std::pair<ElemSide, std::size_t> _elemside_to_batch_index_cache;
};
