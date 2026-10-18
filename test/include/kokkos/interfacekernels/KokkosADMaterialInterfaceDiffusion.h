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

/** Assembles an AD traction supplied by a Kokkos AD interface material. */
class KokkosADMaterialInterfaceDiffusion : public Moose::Kokkos::ADInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosADMaterialInterfaceDiffusion(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION Moose::Kokkos::ADReal computeQpResidual(Moose::DGResidualType type,
                                                          unsigned int i,
                                                          unsigned int qp,
                                                          unsigned int neighbor_qp,
                                                          AssemblyDatum & elem_datum,
                                                          AssemblyDatum & neighbor_datum) const;

private:
  const Moose::Kokkos::MaterialProperty<Moose::Kokkos::ADReal> _traction;
};

template <typename Derived>
KOKKOS_FUNCTION Moose::Kokkos::ADReal
KokkosADMaterialInterfaceDiffusion::computeQpResidual(const Moose::DGResidualType type,
                                                      const unsigned int i,
                                                      const unsigned int qp,
                                                      const unsigned int neighbor_qp,
                                                      AssemblyDatum & elem_datum,
                                                      AssemblyDatum & neighbor_datum) const
{
  const Moose::Kokkos::ADReal traction = _traction(elem_datum, qp);

  if (type == Moose::Element)
    return _test(elem_datum, i, qp) * traction;

  return -_test_neighbor(neighbor_datum, i, neighbor_qp) * traction;
}
