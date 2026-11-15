//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosTractionSeparationLawInterfaceMaterial.h"

registerKokkosInterfaceMaterial("MooseTestApp", KokkosTractionSeparationLawInterfaceMaterial);

InputParameters
KokkosTractionSeparationLawInterfaceMaterial::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.makeParamRequired<std::vector<VariableName>>("variable");
  params.makeParamRequired<std::vector<VariableName>>("neighbor_var");
  params.addRequiredParam<Real>("stiffness", "Traction stiffness of the interface.");
  params.addParam<Real>("state_increment",
                        0.,
                        "Increment added to traction on each accepted step for lifecycle tests.");
  return params;
}

KokkosTractionSeparationLawInterfaceMaterial::KokkosTractionSeparationLawInterfaceMaterial(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _stiffness(getParam<Real>("stiffness")),
    _state_increment(getParam<Real>("state_increment")),
    _traction(declareKokkosPropertyByName<Real>("traction")),
    // Requesting older reserves all three states and puts the property into the shift, copy,
    // rejected-step restoration, and checkpoint code paths.
    _traction_old(getKokkosMaterialPropertyOldByName<Real>("traction")),
    _traction_older(getKokkosMaterialPropertyOlderByName<Real>("traction")),
    _traction_at_old_state(declareKokkosPropertyByName<Real>("traction_at_old_state")),
    _traction_at_older_state(declareKokkosPropertyByName<Real>("traction_at_older_state")),
    _zero_tangent(declareKokkosPropertyByName<Real>("zero_tangent")),
    _dtraction_djump(declareKokkosPropertyByName<Real>("dtraction_djump"))
{
}
