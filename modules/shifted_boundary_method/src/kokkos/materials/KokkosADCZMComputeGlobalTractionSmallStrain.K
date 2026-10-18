//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosADCZMComputeGlobalTractionSmallStrain.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp",
                                KokkosADCZMComputeGlobalTractionSmallStrain);

InputParameters
KokkosADCZMComputeGlobalTractionSmallStrain::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addClassDescription("Rotates AD cohesive traction with Kokkos.");
  return params;
}

KokkosADCZMComputeGlobalTractionSmallStrain::KokkosADCZMComputeGlobalTractionSmallStrain(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ndisp(_mesh.dimension()),
    _czm_total_rotation(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "czm_total_rotation")),
    _interface_traction(getKokkosMaterialPropertyByName<Moose::Kokkos::ADReal, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "interface_traction"))
{
  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _traction_global =
      declareKokkosPropertyByName<Moose::Kokkos::ADReal, 1>(prefix + "traction_global", {_ndisp});
}
