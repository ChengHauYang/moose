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

#ifdef MOOSE_KOKKOS_ENABLED
#include "KokkosArray.h"

namespace Moose::Kokkos
{
class InterfaceFaceCache;
}
#endif

#include <map>
#include <limits>

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

  using ElemSide = std::tuple<dof_id_type, unsigned int>;

  struct BatchSpan
  {
    std::size_t begin;
    std::size_t size;
  };

  /// Key of a batch entry in restartable state: (element ID, side), where the side is
  /// libMesh::invalid_uint for element-interior quadrature points. A pair rather than ElemSide so
  /// that it can be stored as restartable data.
  using StateKey = std::pair<dof_id_type, unsigned int>;
  using StateBatchIndexMap = std::map<StateKey, std::size_t>;

  /// Get the current batch index (in almost all cases this is the total batch size)
  std::size_t getBatchIndex() const { return _batch_index; }

  /// Whether a batch span exists for the given element ID
  bool hasBatchSpan(dof_id_type elem_id) const;

  /// Whether a batch span exists for the given element side
  bool hasSideBatchSpan(const ElemSide & elem_side) const;

  /// Get the batch span for the given element ID
  BatchSpan getBatchSpan(dof_id_type elem_id) const;

  /// Get the batch span for the given element side
  BatchSpan getSideBatchSpan(const ElemSide & elem_side) const;

  /// Get the starting batch index of every volume and side entry, used to remap stateful data
  StateBatchIndexMap getStateBatchIndexMap() const;

  /// Whether the batch is empty
  bool isEmpty() const { return _batch_index == 0; }

  /// Whether the batch index maps must be regenerated
  bool isOutdated() const { return _outdated; }

  /// Generation of the current Kokkos batch-index mirrors
  std::size_t mapGeneration() const { return _map_generation; }

#ifdef MOOSE_KOKKOS_ENABLED
  /// Rebuild and publish the Kokkos batch-index mirrors; called from finalize() after the host
  /// maps are complete. Requires the Kokkos compilation scope and is defined in the .K file.
  void updateKokkosMirrors();

  /// Batch indices indexed by Kokkos contiguous element ID
  const Moose::Kokkos::Array<std::size_t> & kokkosElementBatchIndices() const
  {
    return _kokkos_element_batch_indices;
  }

  /// Primary-side batch indices indexed by Kokkos interface face index
  const Moose::Kokkos::Array<std::size_t> & kokkosFacePrimaryBatchIndices() const
  {
    return _kokkos_face_primary_batch_indices;
  }

  /// Neighbor-side batch indices indexed by Kokkos interface face index
  const Moose::Kokkos::Array<std::size_t> & kokkosFaceNeighborBatchIndices() const
  {
    return _kokkos_face_neighbor_batch_indices;
  }

  /// Invalid entry in a Kokkos batch-index mirror
  static constexpr std::size_t invalidKokkosBatchIndex()
  {
    return std::numeric_limits<std::size_t>::max();
  }
#endif

protected:
  /// Whether element-interior quadrature points contribute to the batch
  const bool _volume;

  /// Whether the batch index map is outdated
  bool _outdated;

  /// Highest current batch index
  std::size_t _batch_index;

  /// Generation of a complete Kokkos batch-index mirror
  std::size_t _map_generation = 0;

#ifdef MOOSE_KOKKOS_ENABLED
  /// Batch indices for volume elements, indexed by contiguous Kokkos element ID
  Moose::Kokkos::Array<std::size_t> _kokkos_element_batch_indices;

  /// Primary-side batch indices, indexed by cached oriented interface face index
  Moose::Kokkos::Array<std::size_t> _kokkos_face_primary_batch_indices;

  /// Neighbor-side batch indices, indexed by cached oriented interface face index
  Moose::Kokkos::Array<std::size_t> _kokkos_face_neighbor_batch_indices;

  /// Shared cache defining the face-indexed interface mirror ordering
  const Moose::Kokkos::InterfaceFaceCache * _interface_face_cache = nullptr;
#endif

  /// Map from element IDs to batch indices
  std::map<dof_id_type, BatchSpan> _elem_to_batch_index;

  /// Map from element sides to batch spans
  std::map<ElemSide, BatchSpan> _elemside_to_batch_index;
};
