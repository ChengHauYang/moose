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
#include "MathUtils.h"

#include <limits>

#ifdef MOOSE_KOKKOS_ENABLED
#include "KokkosElementUserObject.h"
#include "KokkosMaterialProperty.h"

/**
 * Gathers a current-state rank-two Kokkos material property into a contiguous device buffer for
 * direct (zero-copy on CUDA) insertion into a NEML2 input tensor. Full rank-two properties use a
 * nine-component row-major layout; symmetric properties use six-component Mandel packing.
 *
 * Batch row destinations are supplied by NEML2BatchIndexGenerator (execution_order_group = -1),
 * which must finalize before this object's kernel runs in group 0.
 *
 * Only current-state rank-two block material properties are supported in this implementation.
 * Variables, old states, and interface material properties are out of scope.
 *
 * The flat buffer layout is row-major for full tensors: for batch row r and component (i,j), the
 * value is at _buffer[r * 9 + i * 3 + j]. Symmetric tensors use Mandel component order
 * (xx, yy, zz, yz, xz, xy), with sqrt(2) scaling on shear components.
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
   * Device execution: write every component of the rank-two property at each QP into the device
   * buffer, at the batch rows given by the index generator.
   */
  template <typename Derived>
  KOKKOS_FUNCTION void execute(Datum & datum) const;

protected:
  const NEML2BatchIndexGenerator & _batch_index_generator;

  /// Rank-two (3x3) Kokkos material property, indexed as prop(datum, qp)(i, j)
  Moose::Kokkos::MaterialProperty<Real, 2> _mat_prop;

  /// Whether to gather symmetric properties using six-component Mandel packing.
  const bool _symmetric;

  /// Number of scalar components in each gathered NEML2 batch row.
  const unsigned int _num_components;

  /// Device-accessible copy of the element batch indices from the generator.
  /// Refreshed each compute() after the generator updates its Kokkos mirrors.
  Moose::Kokkos::Array<std::size_t> _elem_batch_indices;

  /// Flat device buffer: [total_qp_rows * _num_components] doubles
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
    const auto row = (elem_batch + qp) * _num_components;
    if (_symmetric)
    {
      _buffer(row) = val(0, 0);
      _buffer(row + 1) = val(1, 1);
      _buffer(row + 2) = val(2, 2);
      _buffer(row + 3) = MathUtils::sqrt2 * val(1, 2);
      _buffer(row + 4) = MathUtils::sqrt2 * val(0, 2);
      _buffer(row + 5) = MathUtils::sqrt2 * val(0, 1);
    }
    else
      for (unsigned int i = 0; i < 3; ++i)
        for (unsigned int j = 0; j < 3; ++j)
          _buffer(row + i * 3 + j) = val(i, j);
  }
}

#endif // MOOSE_KOKKOS_ENABLED
