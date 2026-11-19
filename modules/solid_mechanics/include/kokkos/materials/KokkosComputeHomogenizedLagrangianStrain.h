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
#include "HomogenizationInterface.h"

/**
 * Kokkos version of ComputeHomogenizedLagrangianStrain: writes the components of the macro
 * gradient scalar variable into a rank-two property through the homogenization constraint map.
 */
class KokkosComputeHomogenizedLagrangianStrain
  : public HomogenizationInterface<Moose::Kokkos::Material>
{
public:
  static InputParameters validParams();

  KokkosComputeHomogenizedLagrangianStrain(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const;

private:
  /// The macro gradient scalar variable
  const Moose::Kokkos::VariableValue _macro_gradient;
  /// Tensor indices (i, j) of each macro gradient component, in constraint map order
  ///@{
  Moose::Kokkos::Array<unsigned int> _index_i;
  Moose::Kokkos::Array<unsigned int> _index_j;
  ///@}
  /// The homogenization gradient
  Moose::Kokkos::MaterialProperty<Real, 2> _homogenization_gradient;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosComputeHomogenizedLagrangianStrain::computeQpProperties(const unsigned int qp,
                                                              Datum & datum) const
{
  auto H = _homogenization_gradient(datum, qp);
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      H(i, j) = 0;

  for (unsigned int h = 0; h < _index_i.size(); ++h)
    H(_index_i[h], _index_j[h]) = _macro_gradient(datum, h);
}
