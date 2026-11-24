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
 * Gathers a current-state vector Kokkos interface material property, which has one value per
 * interface quadrature point (e.g. the displacement jump of a cohesive zone), into a contiguous
 * device buffer for direct (zero-copy on CUDA) insertion into a NEML2 input tensor.
 *
 * This is the Kokkos counterpart of a host MOOSEQuantityToNEML2 with from_interface_material =
 * true. Batch row destinations are supplied by an interface-only NEML2BatchIndexGenerator
 * (volume = false), which must finalize in an earlier execution_order_group because Kokkos user
 * objects run before host user objects within a group. The single interface value of each QP is
 * written to both the primary-side and, where it is batched, the neighbor-side row, so both sides
 * evaluate the model at the same interface value.
 *
 * The flat buffer layout is row-major: for batch row r and component i, the value is at
 * _buffer[r * 3 + i].
 */
class KokkosInterfaceMaterialVectorToNEML2 : public Moose::Kokkos::InterfaceUserObject,
                                             public MOOSEToNEML2
{
public:
  static InputParameters validParams();

  KokkosInterfaceMaterialVectorToNEML2(const InputParameters & parameters);

  KokkosInterfaceMaterialVectorToNEML2(const KokkosInterfaceMaterialVectorToNEML2 & object);

  using InterfaceUserObject::operator();

  void initialize() override;
  void compute() override;
  void finalize() override;

#ifdef NEML2_ENABLED
  at::Tensor gatheredData() const override;
#endif

  /**
   * Device execution: write the 3 components of the interface property at each QP of this
   * interface face into the device buffer, at the batch rows given by the index generator's
   * primary and neighbor face maps.
   */
  template <typename Derived>
  KOKKOS_FUNCTION void executeInterface(Datum & primary,
                                        Datum & neighbor,
                                        const Moose::Kokkos::InterfaceFaceRecord & face) const;

protected:
  const NEML2BatchIndexGenerator & _batch_index_generator;

  /// Vector Kokkos interface material property (one value per interface QP)
  Moose::Kokkos::MaterialProperty<Real, 1> _mat_prop;

  /// Device-accessible copy of primary-side face batch indices from the generator.
  Moose::Kokkos::Array<std::size_t> _face_primary_batch_indices;
  /// Device-accessible copy of neighbor-side face batch indices from the generator.
  Moose::Kokkos::Array<std::size_t> _face_neighbor_batch_indices;

  /// Flat device buffer: [total_qp_rows * 3] doubles in row-major order
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
KokkosInterfaceMaterialVectorToNEML2::executeInterface(
    Datum & primary, Datum & /*neighbor*/, const Moose::Kokkos::InterfaceFaceRecord & face) const
{
  const auto invalid = NEML2BatchIndexGenerator::invalidKokkosBatchIndex();
  const auto pri_batch = _face_primary_batch_indices(face.face_index);
  const auto nbr_batch = _face_neighbor_batch_indices(face.face_index);

  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
  {
    const auto & val = _mat_prop(primary, qp);
    const unsigned int n_comp = val.n(0);
    KOKKOS_ASSERT(n_comp <= 3);
    // Batch rows of both sides are keyed by the primary QP; 1D/2D vectors pad trailing entries
    // with zero to match the 3-component NEML2 vector layout.
    if (pri_batch != invalid)
      for (unsigned int i = 0; i < 3; ++i)
        _buffer((pri_batch + qp) * 3 + i) = i < n_comp ? val(i) : 0.0;
    if (nbr_batch != invalid)
      for (unsigned int i = 0; i < 3; ++i)
        _buffer((nbr_batch + qp) * 3 + i) = i < n_comp ? val(i) : 0.0;
  }
}

#endif // MOOSE_KOKKOS_ENABLED
