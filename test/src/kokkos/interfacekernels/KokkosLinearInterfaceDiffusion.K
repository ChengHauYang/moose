//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosLinearInterfaceDiffusion.h"

registerKokkosInterfaceKernel("MooseTestApp", KokkosLinearInterfaceDiffusion);

InputParameters
KokkosLinearInterfaceDiffusion::validParams()
{
  auto params = InterfaceKernel::validParams();
  params.addRequiredParam<MaterialPropertyName>("traction", "Traction supplied by the material.");
  params.addRequiredParam<MaterialPropertyName>(
      "dtraction_djump", "Derivative of the traction with respect to the jump.");
  return params;
}

KokkosLinearInterfaceDiffusion::KokkosLinearInterfaceDiffusion(const InputParameters & parameters)
  : InterfaceKernel(parameters),
    _traction(getKokkosMaterialProperty<Real>("traction")),
    _dtraction_djump(getKokkosMaterialProperty<Real>("dtraction_djump"))
{
}
