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
#include "KokkosElementUserObject.h"
#include "KokkosMaterialProperty.h"

/**
 * Gathers a current-state full rank-two Kokkos material property into a contiguous
 * device buffer for direct (zero-copy on CUDA) insertion into a NEML2 input tensor.
 *
 * Batch row destinations are supplied by NEML2BatchIndexGenerator (execution_order_group = -1),
 * which must finalize before this object's kernel runs in group 0.
 *
 * Only current-state full rank-two block material properties are supported in this
 * implementation. Variables, old states, and interface material properties are out of scope.
 *
 * The flat buffer layout is row-major: for batch row r and component (i,j),
 * the value is at _buffer[r * 9 + i * 3 + j].
 */
class KokkosQuantityToNEML2 : public Moose::Kokkos::ElementUserObject, public MOOSEToNEML2
{
public:
  static InputParameters validParams();

  KokkosQuantityToNEML2(const InputParameters & parameters);

  KokkosQuantityToNEML2(const KokkosQuantityToNEML2 & object);

  void initialize() override;
  void compute() override;
  void finalize() override;

#ifdef NEML2_ENABLED
  at::Tensor gatheredData() const override;
#endif

  /**
   * Device execution: write all 9 row-major components of the rank-two property at each
   * QP of this element into the device buffer, at the batch rows given by the index generator.
   */
  template <typename Derived>
  KOKKOS_FUNCTION void execute(Datum & datum) const;

protected:
  const NEML2BatchIndexGenerator & _batch_index_generator;

  /// Full rank-two (3x3) Kokkos material property, indexed as prop(datum, qp)(i, j)
  Moose::Kokkos::MaterialProperty<Real, 2> _mat_prop;

  /// Device-accessible copy of the element batch indices from the generator.
  /// Refreshed each compute() after the generator updates its Kokkos mirrors.
  Moose::Kokkos::Array<std::size_t> _elem_batch_indices;

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
KokkosQuantityToNEML2::execute(Datum & datum) const
{
  const auto invalid = NEML2BatchIndexGenerator::invalidKokkosBatchIndex();
  const auto elem_batch = _elem_batch_indices(datum.elemID());
  if (elem_batch == invalid)
    return;

  const auto n_qps = datum.n_qps();
  for (unsigned int qp = 0; qp < n_qps; ++qp)
  {
    const auto & val = _mat_prop(datum, qp);
    const auto row = (elem_batch + qp) * 9;
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
        _buffer(row + i * 3 + j) = val(i, j);
  }
}

#endif // MOOSE_KOKKOS_ENABLED
