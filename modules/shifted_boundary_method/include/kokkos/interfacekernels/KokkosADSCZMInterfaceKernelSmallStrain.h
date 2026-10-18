//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosADShiftedInterfaceKernel.h"

/** AD Kokkos shifted cohesive-zone interface kernel for small-strain displacements. */
class KokkosADSCZMInterfaceKernelSmallStrain : public Moose::Kokkos::ADShiftedInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosADSCZMInterfaceKernelSmallStrain(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION Moose::Kokkos::ADReal computeQpResidual(Moose::DGResidualType type,
                                                          unsigned int i,
                                                          unsigned int qp,
                                                          unsigned int neighbor_qp,
                                                          AssemblyDatum & elem_datum,
                                                          AssemblyDatum & neighbor_datum) const;

private:
  const unsigned int _component;
  const bool _shifted;
  const Moose::Kokkos::MaterialProperty<Moose::Kokkos::ADReal, 1> _traction_global;
};

template <typename Derived>
KOKKOS_FUNCTION Moose::Kokkos::ADReal
KokkosADSCZMInterfaceKernelSmallStrain::computeQpResidual(const Moose::DGResidualType type,
                                                          const unsigned int i,
                                                          const unsigned int qp,
                                                          const unsigned int neighbor_qp,
                                                          AssemblyDatum & elem_datum,
                                                          AssemblyDatum & neighbor_datum) const
{
  const auto test = type == Moose::Element ? _test(elem_datum, i, qp)
                                           : _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto sign = type == Moose::Element ? -1.0 : 1.0;
  const auto traction = _traction_global(elem_datum, qp)(_component);

  if (!_shifted)
    return sign * test * traction;

  return sign * test * areaCorrection(elem_datum, qp) * traction;
}
