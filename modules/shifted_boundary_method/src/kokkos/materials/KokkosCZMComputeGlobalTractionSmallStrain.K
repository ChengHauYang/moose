//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosCZMComputeGlobalTractionSmallStrain.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp",
                                KokkosCZMComputeGlobalTractionSmallStrain);

InputParameters
KokkosCZMComputeGlobalTractionSmallStrain::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addClassDescription("Rotates cohesive traction and its tangent with Kokkos.");
  return params;
}

KokkosCZMComputeGlobalTractionSmallStrain::KokkosCZMComputeGlobalTractionSmallStrain(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters), _ndisp(_mesh.dimension())
{
  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _czm_total_rotation = getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_total_rotation");
  _interface_traction = getKokkosMaterialPropertyByName<Real, 1>(prefix + "interface_traction");
  _dinterface_traction_djump =
      getKokkosMaterialPropertyByName<Real, 2>(prefix + "dinterface_traction_djump");
  _traction_global = declareKokkosPropertyByName<Real, 1>(prefix + "traction_global", {_ndisp});
  _dtraction_djump_global =
      declareKokkosPropertyByName<Real, 2>(prefix + "dtraction_djump_global", {_ndisp, _ndisp});
}
