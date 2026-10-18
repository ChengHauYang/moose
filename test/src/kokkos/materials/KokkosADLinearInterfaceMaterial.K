//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosADLinearInterfaceMaterial.h"

registerKokkosInterfaceMaterial("MooseTestApp", KokkosADLinearInterfaceMaterial);

InputParameters
KokkosADLinearInterfaceMaterial::validParams()
{
  auto params = ADInterfaceMaterial::validParams();
  params.makeParamRequired<std::vector<VariableName>>("variable");
  params.makeParamRequired<std::vector<VariableName>>("neighbor_var");
  params.addParam<Real>("stiffness", 1, "Traction stiffness of the interface.");
  return params;
}

KokkosADLinearInterfaceMaterial::KokkosADLinearInterfaceMaterial(const InputParameters & parameters)
  : ADInterfaceMaterial(parameters),
    _stiffness(getParam<Real>("stiffness")),
    _traction(declareKokkosPropertyByName<Moose::Kokkos::ADReal>("traction"))
{
}
