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

/// Provides constant device-resident traction and cohesive tangent properties for testing.
class KokkosLinearCohesiveZoneMaterial : public Moose::Kokkos::Material
{
public:
  static InputParameters validParams();

  KokkosLinearCohesiveZoneMaterial(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const
  {
    auto traction = _traction_global(datum, qp);
    auto tangent = _dtraction_djump_global(datum, qp);

    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      traction(i) = 0;
      for (unsigned int j = 0; j < _ndisp; ++j)
        tangent(i, j) = _stiffness(i, j);
    }
  }

private:
  const unsigned int _ndisp;
  Moose::Kokkos::Array2D<Real> _stiffness;
  Moose::Kokkos::MaterialProperty<Real, 1> _traction_global;
  Moose::Kokkos::MaterialProperty<Real, 2> _dtraction_djump_global;
};
