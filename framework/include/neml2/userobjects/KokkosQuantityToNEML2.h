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

#include "KokkosElementUserObject.h"
#include "KokkosArray.h"
#include "KokkosVariableValue.h"

namespace Moose::Kokkos
{
template <typename T>
struct QuantityTraits;

template <>
struct QuantityTraits<Real>
{
  using ValueType = Moose::Kokkos::VariableValue;
  using BufferType = Real;
};

template <>
struct QuantityTraits<RealVectorValue>
{
  using ValueType = Moose::Kokkos::VectorVariableValue;
  using BufferType = Moose::Kokkos::Real3;
};
}

/**
 * Gather a MOOSE field variable into a persistent device buffer for insertion into the NEML2
 * model, without round-tripping through the host.
 *
 * The gathered data layout is element-major and quadrature-point-minor over the contiguously
 * numbered local elements, matching the layout produced by the host-side MOOSEQuantityToNEML2
 * gatherer. Each element's quadrature point values are contiguous, starting at the running
 * offset recorded in _element_batch_offset.
 */
template <typename T, unsigned int state>
class KokkosQuantityToNEML2 : public Moose::Kokkos::ElementUserObject, public MOOSEToNEML2
{
public:
  static_assert(state <= 2,
                "KokkosQuantityToNEML2 only supports state <= 2 (current, old, older).");

  static InputParameters validParams();

  KokkosQuantityToNEML2(const InputParameters & params);

  /**
   * Copy constructor for parallel dispatch
   */
  KokkosQuantityToNEML2(const KokkosQuantityToNEML2 & object);

  /**
   * Rebuild the element-to-batch offset mapping and the persistent gather buffer on each
   * execution pass if the mesh or the number of quadrature points changed.
   */
  void initialize() override;

  void finalize() override {}

  /**
   * Device kernel that gathers the coupled variable's quadrature point values into the
   * persistent device-resident gather buffer
   * @param datum The Datum object of the current thread
   */
  template <typename Derived>
  KOKKOS_FUNCTION void execute(Datum & datum) const;

#ifdef NEML2_ENABLED
  at::Tensor gatheredData() const override;
#endif

protected:
  using ValueType = typename Moose::Kokkos::QuantityTraits<T>::ValueType;
  using BufferType = typename Moose::Kokkos::QuantityTraits<T>::BufferType;

  /// The coupled variable field sampled by the gather kernel
  ValueType _v;

  /// Persistent device-resident gather buffer, contiguous over elements (quadrature points within
  /// an element)
  Moose::Kokkos::Array<BufferType> _buffer;

  /// Offset into _buffer of the first quadrature point of each contiguously numbered local element
  Moose::Kokkos::Array<dof_id_type> _element_batch_offset;
};

template <typename T, unsigned int state>
template <typename Derived>
KOKKOS_FUNCTION void
KokkosQuantityToNEML2<T, state>::execute(Datum & datum) const
{
  const auto offset = _element_batch_offset[datum.elemID()];
  for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
    _buffer[offset + qp] = _v(datum, qp);
}

using KokkosRealToNEML2 = KokkosQuantityToNEML2<Real, 0>;
using KokkosOldRealToNEML2 = KokkosQuantityToNEML2<Real, 1>;
using KokkosOlderRealToNEML2 = KokkosQuantityToNEML2<Real, 2>;

using KokkosRealVectorValueToNEML2 = KokkosQuantityToNEML2<RealVectorValue, 0>;
using KokkosOldRealVectorValueToNEML2 = KokkosQuantityToNEML2<RealVectorValue, 1>;
using KokkosOlderRealVectorValueToNEML2 = KokkosQuantityToNEML2<RealVectorValue, 2>;
