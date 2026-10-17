//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosADLinearInterfaceDiffusion.h"

registerKokkosADInterfaceKernel("MooseTestApp", KokkosADLinearInterfaceDiffusion);

InputParameters
KokkosADLinearInterfaceDiffusion::validParams()
{
  auto params = ADInterfaceKernel::validParams();
  params.addParam<Real>("penalty", 1, "Penalty applied to the solution jump.");
  return params;
}

KokkosADLinearInterfaceDiffusion::KokkosADLinearInterfaceDiffusion(
    const InputParameters & parameters)
  : ADInterfaceKernel(parameters), _penalty(getParam<Real>("penalty"))
{
}
