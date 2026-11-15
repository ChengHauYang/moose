//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosMaterial.h"

/**
 * Declares a full rank-two (3x3) Kokkos material property with constant row-major values
 * specified by input, optionally scaled by an affine spatial function. Used to supply
 * a known tensor to KokkosQuantityToNEML2 tests.
 */
class KokkosConstantRankTwoMaterial : public Moose::Kokkos::Material
{
public:
  static InputParameters validParams();

  KokkosConstantRankTwoMaterial(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const;

protected:
  Moose::Kokkos::MaterialProperty<Real, 2> _prop;

  /// 9 row-major component values stored on device
  Moose::Kokkos::Array<Real> _values;

  /// Slopes in the common tensor scale 1 + x_slope * x + y_slope * y.
  const Real _x_slope;
  const Real _y_slope;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosConstantRankTwoMaterial::computeQpProperties(const unsigned int qp, Datum & datum) const
{
  auto val = _prop(datum, qp);
  const auto point = datum.q_point(qp);
  const auto scale = 1.0 + _x_slope * point(0) + _y_slope * point(1);
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      val(i, j) = scale * _values(i * 3 + j);
}
