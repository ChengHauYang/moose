//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosADInterfaceMaterial.h"

/** Provides an AD traction property from the scalar jump across an interface. */
class KokkosADLinearInterfaceMaterial : public Moose::Kokkos::ADInterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosADLinearInterfaceMaterial(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           AssemblyDatum & elem_datum,
                                           AssemblyDatum & neighbor_datum,
                                           unsigned int qp,
                                           unsigned int neighbor_qp) const;

private:
  const Real _stiffness;
  Moose::Kokkos::MaterialProperty<Moose::Kokkos::ADReal> _traction;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosADLinearInterfaceMaterial::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord & /* face */,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  _traction(elem_datum, qp) =
      _stiffness * (_ad_variable_value(elem_datum, qp) -
                    _ad_neighbor_variable_value(neighbor_datum, neighbor_qp));
}
