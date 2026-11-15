//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSCZMComputeGlobalTractionTotalLagrangian.h"

#include "BoundaryShortestDistanceToSurface.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp",
                                KokkosSCZMComputeGlobalTractionTotalLagrangian);

InputParameters
KokkosSCZMComputeGlobalTractionTotalLagrangian::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addParam<UserObjectName>("sbm_distance_uo",
                                  "UserObject providing the surrogate-interface mapping data.");
  params.addParam<bool>("no_shifted", false, "Disable shifted integration corrections.");
  params.addClassDescription("Computes total-Lagrangian shifted cohesive PK1 traction and "
                             "derivatives with Kokkos.");
  return params;
}

KokkosSCZMComputeGlobalTractionTotalLagrangian::KokkosSCZMComputeGlobalTractionTotalLagrangian(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ndisp(_mesh.dimension()),
    _shifted(!getParam<bool>("no_shifted")),
    _sbm_distance_uo(_shifted && isParamValid("sbm_distance_uo")
                         ? &getUserObject<BoundaryShortestDistanceToSurface>("sbm_distance_uo")
                         : nullptr)
{
  if (_shifted && !isParamValid("sbm_distance_uo"))
    paramError("sbm_distance_uo",
               "The 'sbm_distance_uo' parameter is required unless 'no_shifted = true'.");

  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _displacement_jump_global =
      getKokkosMaterialPropertyByName<Real, 1>(prefix + "displacement_jump_global");
  _czm_total_rotation = getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_total_rotation");
  _czm_reference_rotation =
      getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_reference_rotation");
  _czm_rotation = getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_rotation");
  _F_czm = getKokkosMaterialPropertyByName<Real, 2>(prefix + "F_czm");
  _interface_traction = getKokkosMaterialPropertyByName<Real, 1>(prefix + "interface_traction");
  _dinterface_traction_djump =
      getKokkosMaterialPropertyByName<Real, 2>(prefix + "dinterface_traction_djump");

  _traction_global = declareKokkosPropertyByName<Real, 1>(prefix + "traction_global", {_ndisp});
  _PK1traction = declareKokkosPropertyByName<Real, 1>(prefix + "PK1traction", {_ndisp});
  _dtraction_djump_global =
      declareKokkosPropertyByName<Real, 2>(prefix + "dtraction_djump_global", {_ndisp, _ndisp});
  _dPK1traction_dF =
      declareKokkosPropertyByName<Real, 3>(prefix + "dPK1traction_dF", {_ndisp, _ndisp, _ndisp});

  if (_shifted)
    _fe_problem.addKokkosMeshInitializationHook(std::bind(
        &KokkosSCZMComputeGlobalTractionTotalLagrangian::initializeSurrogateInterfaceMapping,
        this));
}

KokkosSCZMComputeGlobalTractionTotalLagrangian::KokkosSCZMComputeGlobalTractionTotalLagrangian(
    const KokkosSCZMComputeGlobalTractionTotalLagrangian & object)
  : InterfaceMaterial(object),
    _ndisp(object._ndisp),
    _shifted(object._shifted),
    _sbm_distance_uo(object._sbm_distance_uo),
    _true_normals(object._true_normals),
    _displacement_jump_global(object._displacement_jump_global),
    _czm_total_rotation(object._czm_total_rotation),
    _czm_reference_rotation(object._czm_reference_rotation),
    _czm_rotation(object._czm_rotation),
    _F_czm(object._F_czm),
    _interface_traction(object._interface_traction),
    _dinterface_traction_djump(object._dinterface_traction_djump),
    _traction_global(object._traction_global),
    _PK1traction(object._PK1traction),
    _dtraction_djump_global(object._dtraction_djump_global),
    _dPK1traction_dF(object._dPK1traction_dF)
{
}

void
KokkosSCZMComputeGlobalTractionTotalLagrangian::computeProperties()
{
  if (_shifted && _sbm_distance_uo &&
      !_sbm_distance_uo->kokkosSurrogateInterfaceMappingReady(boundaryIDs()))
    mooseError("Surrogate-interface mapping for '",
               name(),
               "' is unavailable. Ensure its 'sbm_distance_uo' executes before material "
               "evaluation.");
  InterfaceMaterial::computeProperties();
}

void
KokkosSCZMComputeGlobalTractionTotalLagrangian::initializeSurrogateInterfaceMapping()
{
  if (_shifted && _sbm_distance_uo)
  {
    const auto & mapping = _sbm_distance_uo->registerKokkosSurrogateInterfaceMapping(
        boundaryIDs(), kokkosInterfaceFaces());
    _true_normals = mapping.true_normals;
  }
}
