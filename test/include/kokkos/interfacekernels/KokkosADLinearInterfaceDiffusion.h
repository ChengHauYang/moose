//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosADInterfaceKernel.h"

/** Applies a linear penalty to the solution jump across a Kokkos interface. */
class KokkosADLinearInterfaceDiffusion : public Moose::Kokkos::ADInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosADLinearInterfaceDiffusion(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION Moose::Kokkos::ADReal computeQpResidual(Moose::DGResidualType type,
                                                          unsigned int i,
                                                          unsigned int qp,
                                                          unsigned int neighbor_qp,
                                                          AssemblyDatum & elem_datum,
                                                          AssemblyDatum & neighbor_datum) const;

private:
  const Real _penalty;
};

template <typename Derived>
KOKKOS_FUNCTION Moose::Kokkos::ADReal
KokkosADLinearInterfaceDiffusion::computeQpResidual(const Moose::DGResidualType type,
                                                    const unsigned int i,
                                                    const unsigned int qp,
                                                    const unsigned int neighbor_qp,
                                                    AssemblyDatum & elem_datum,
                                                    AssemblyDatum & neighbor_datum) const
{
  const auto jump = _u(elem_datum, qp) - _u_neighbor(neighbor_datum, neighbor_qp);

  if (type == Moose::Element)
    return _test(elem_datum, i, qp) * _penalty * jump;

  return -_test_neighbor(neighbor_datum, i, neighbor_qp) * _penalty * jump;
}
