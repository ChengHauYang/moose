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
 * Kokkos version of RankTwoCartesianComponent: copies one component of a rank-two Kokkos material
 * property into a scalar Kokkos material property.
 */
class KokkosRankTwoCartesianComponent : public Moose::Kokkos::Material
{
public:
  static InputParameters validParams();

  KokkosRankTwoCartesianComponent(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const
  {
    _property(datum, qp) = _tensor(datum, qp)(_i, _j);
  }

private:
  Moose::Kokkos::MaterialProperty<Real, 2> _tensor;
  const unsigned int _i;
  const unsigned int _j;
  Moose::Kokkos::MaterialProperty<Real, 0> _property;
};
