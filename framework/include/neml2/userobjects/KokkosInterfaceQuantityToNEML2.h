//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MOOSEToNEML2.h"
#include "NEML2BatchIndexGenerator.h"

#include <limits>

#ifdef MOOSE_KOKKOS_ENABLED
#include "KokkosInterfaceUserObject.h"
#include "KokkosMaterialProperty.h"

/**
 * Gathers current-state full rank-two Kokkos interface material properties from both
 * sides of a cohesive interface into a contiguous device buffer for direct (zero-copy
 * on CUDA) insertion into a NEML2 input tensor.
 *
 * Batch row destinations are supplied by NEML2BatchIndexGenerator with interface_only = true
 * (execution_order_group = -1), which must finalize before this object's kernel runs in group 0.
 * Primary-side QP rows are written at kokkosFacePrimaryBatchIndices offsets; neighbor-side
 * QP rows at kokkosFaceNeighborBatchIndices offsets.
 *
 * The flat buffer layout is row-major: for batch row r and component (i,j),
 * the value is at _buffer[r * 9 + i * 3 + j].
 */
class KokkosInterfaceQuantityToNEML2 : public Moose::Kokkos::InterfaceUserObject,
                                       public MOOSEToNEML2
{
public:
  static InputParameters validParams();

  KokkosInterfaceQuantityToNEML2(const InputParameters & parameters);

  KokkosInterfaceQuantityToNEML2(const KokkosInterfaceQuantityToNEML2 & object);

  using InterfaceUserObject::operator();

  void initialize() override;
  void compute() override;
  void finalize() override;

#ifdef NEML2_ENABLED
  at::Tensor gatheredData() const override;
#endif

  /**
   * Device execution: write all 9 row-major components of both sides' rank-two properties
   * at each QP pair of this interface face into the device buffer, at the batch rows given
   * by the index generator's primary and neighbor face maps.
   */
  template <typename Derived>
  KOKKOS_FUNCTION void executeInterface(Datum & primary,
                                        Datum & neighbor,
                                        const Moose::Kokkos::InterfaceFaceRecord & face) const;

protected:
  const NEML2BatchIndexGenerator & _batch_index_generator;

  /// Primary-side full rank-two Kokkos interface material property
  Moose::Kokkos::MaterialProperty<Real, 2> _primary_mat_prop;
  /// Neighbor-side full rank-two Kokkos interface material property
  Moose::Kokkos::MaterialProperty<Real, 2> _neighbor_mat_prop;

  /// Device-accessible copy of primary-side face batch indices from the generator.
  Moose::Kokkos::Array<std::size_t> _face_primary_batch_indices;
  /// Device-accessible copy of neighbor-side face batch indices from the generator.
  Moose::Kokkos::Array<std::size_t> _face_neighbor_batch_indices;

  /// Flat device buffer: [total_qp_rows * 9] doubles in row-major order
  Moose::Kokkos::Array<double> _buffer;

  /// Generation of the batch-index mirror at the last buffer allocation.
  /// Initialized to max so the first execute always allocates the buffer.
  std::size_t _last_map_generation = std::numeric_limits<std::size_t>::max();

#ifdef NEML2_ENABLED
  /// Torch tensor wrapping _buffer; rebuilt each finalize()
  at::Tensor _gathered_tensor;
#endif
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosInterfaceQuantityToNEML2::executeInterface(
    Datum & primary, Datum & neighbor, const Moose::Kokkos::InterfaceFaceRecord & face) const
{
  const auto invalid = NEML2BatchIndexGenerator::invalidKokkosBatchIndex();
  const auto pri_batch = _face_primary_batch_indices(face.face_index);
  const auto nbr_batch = _face_neighbor_batch_indices(face.face_index);

  const auto n_qps = face.n_qps;
  for (unsigned int qp = 0; qp < n_qps; ++qp)
  {
    if (pri_batch != invalid)
    {
      const auto & pval = _primary_mat_prop(primary, qp);
      const auto prow = (pri_batch + qp) * 9;
      for (unsigned int i = 0; i < 3; ++i)
        for (unsigned int j = 0; j < 3; ++j)
          _buffer(prow + i * 3 + j) = pval(i, j);
    }

    const auto neighbor_qp = kokkosInterfaceNeighborQp(face, qp);
    if (nbr_batch != invalid)
    {
      const auto & nval = _neighbor_mat_prop(neighbor, neighbor_qp);
      const auto nrow = (nbr_batch + qp) * 9;
      for (unsigned int i = 0; i < 3; ++i)
        for (unsigned int j = 0; j < 3; ++j)
          _buffer(nrow + i * 3 + j) = nval(i, j);
    }
  }
}

#endif // MOOSE_KOKKOS_ENABLED
