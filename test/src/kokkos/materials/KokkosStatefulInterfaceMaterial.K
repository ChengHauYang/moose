//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosStatefulInterfaceMaterial.h"

registerKokkosInterfaceMaterial("MooseTestApp", KokkosStatefulInterfaceMaterial);

InputParameters
KokkosStatefulInterfaceMaterial::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.makeParamRequired<std::vector<VariableName>>("variable");
  params.makeParamRequired<std::vector<VariableName>>("neighbor_var");
  params.addRequiredParam<Real>("stiffness", "Traction stiffness of the interface.");
  return params;
}

KokkosStatefulInterfaceMaterial::KokkosStatefulInterfaceMaterial(const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _stiffness(getParam<Real>("stiffness")),
    _traction(declareKokkosPropertyByName<Real>("traction")),
    // Requesting the old state reserves the storages up to state one and marks the property
    // stateful, which is what puts it into the shift, copy, and checkpoint code paths.
    _traction_old(getKokkosMaterialPropertyOldByName<Real>("traction")),
    _dtraction_djump(declareKokkosPropertyByName<Real>("dtraction_djump"))
{
}
