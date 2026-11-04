//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosPureElasticTractionSeparation.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp", KokkosPureElasticTractionSeparation);

InputParameters
KokkosPureElasticTractionSeparation::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addRequiredParam<Real>("normal_stiffness", "Stiffness in the normal direction.");
  params.addRequiredParam<Real>("tangent_stiffness", "Stiffness in tangential directions.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addClassDescription(
      "Computes a pure elastic cohesive traction-separation law with Kokkos.");
  return params;
}

KokkosPureElasticTractionSeparation::KokkosPureElasticTractionSeparation(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ndisp(_mesh.dimension()),
    _normal_stiffness(getParam<Real>("normal_stiffness")),
    _tangent_stiffness(getParam<Real>("tangent_stiffness")),
    _interface_displacement_jump(getKokkosMaterialPropertyByName<Real, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "interface_displacement_jump"))
{
  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _interface_traction =
      declareKokkosPropertyByName<Real, 1>(prefix + "interface_traction", {_ndisp});
  _dinterface_traction_djump =
      declareKokkosPropertyByName<Real, 2>(prefix + "dinterface_traction_djump", {_ndisp, _ndisp});
}
