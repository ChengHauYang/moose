//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosLinearSCZMInterfaceKernel.h"

registerKokkosInterfaceKernel("ShiftedBoundaryMethodTestApp", KokkosLinearSCZMInterfaceKernel);

InputParameters
KokkosLinearSCZMInterfaceKernel::validParams()
{
  auto params = KokkosSCZMInterfaceKernelSmallStrain::validParams();
  params.addClassDescription("Tests Kokkos SCZM assembly with a linear cohesive law.");
  return params;
}

KokkosLinearSCZMInterfaceKernel::KokkosLinearSCZMInterfaceKernel(
    const InputParameters & parameters)
  : KokkosSCZMInterfaceKernelSmallStrain(parameters),
    _displacements(kokkosCoupledValues("displacements")),
    _displacement_gradients(kokkosCoupledGradients("displacements"))
{
}
