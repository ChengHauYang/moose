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
 * Computes the infinitesimal strain as a Kokkos rank-two material property.
 */
class KokkosComputeSmallStrain : public Moose::Kokkos::Material
{
public:
  static InputParameters validParams();

  KokkosComputeSmallStrain(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const;

private:
  const unsigned int _ndisp;
  const Moose::Kokkos::VariableGradient _grad_displacements;
  Moose::Kokkos::MaterialProperty<Real, 2> _strain;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosComputeSmallStrain::computeQpProperties(const unsigned int qp, Datum & datum) const
{
  auto strain = _strain(datum, qp);
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      strain(i, j) =
          i < _ndisp && j < _ndisp
              ? 0.5 * (_grad_displacements(datum, qp, i)(j) + _grad_displacements(datum, qp, j)(i))
              : 0.0;
}
