//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSCZMComputeDisplacementJumpSmallStrain.h"

#include "BoundaryShortestDistanceToSurface.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp",
                                KokkosSCZMComputeDisplacementJumpSmallStrain);

InputParameters
KokkosSCZMComputeDisplacementJumpSmallStrain::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addRequiredCoupledVar("displacements", "Displacement variables for the problem.");
  params.addRequiredParam<UserObjectName>(
      "sbm_distance_uo", "UserObject providing the surrogate-interface mapping data.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addParam<bool>(
      "no_shifted",
      false,
      "Disable shifted terms. When enabled the displacement jump omits the gradient contribution "
      "and the reference rotation is built from the surrogate face normal.");
  params.addClassDescription(
      "Computes the shifted small-strain cohesive displacement jump with Kokkos.");
  return params;
}

KokkosSCZMComputeDisplacementJumpSmallStrain::KokkosSCZMComputeDisplacementJumpSmallStrain(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ndisp(coupledComponents("displacements")),
    _shifted(!getParam<bool>("no_shifted")),
    _sbm_distance_uo(getUserObject<BoundaryShortestDistanceToSurface>("sbm_distance_uo")),
    _displacements(kokkosCoupledValues("displacements")),
    _displacement_gradients(kokkosCoupledGradients("displacements"))
{
  if (_ndisp != _mesh.dimension())
    paramError("displacements", "Number of displacements must match problem dimension.");
  if (_ndisp < 1 || _ndisp > 3)
    paramError("displacements", "One, two, or three displacement variables are required.");

  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _displacement_jump_global =
      declareKokkosPropertyByName<Real, 1>(prefix + "displacement_jump_global", {_ndisp});
  _interface_displacement_jump =
      declareKokkosPropertyByName<Real, 1>(prefix + "interface_displacement_jump", {_ndisp});
  _czm_total_rotation =
      declareKokkosPropertyByName<Real, 2>(prefix + "czm_total_rotation", {_ndisp, _ndisp});

  _fe_problem.addKokkosMeshInitializationHook(std::bind(
      &KokkosSCZMComputeDisplacementJumpSmallStrain::initializeSurrogateInterfaceMapping, this));
}

KokkosSCZMComputeDisplacementJumpSmallStrain::KokkosSCZMComputeDisplacementJumpSmallStrain(
    const KokkosSCZMComputeDisplacementJumpSmallStrain & object)
  : InterfaceMaterial(object),
    _ndisp(object._ndisp),
    _shifted(object._shifted),
    _sbm_distance_uo(object._sbm_distance_uo),
    _displacements(object._displacements),
    _displacement_gradients(object._displacement_gradients),
    _surrogate_distance_vectors(object._surrogate_distance_vectors),
    _rotations(object._rotations),
    _displacement_jump_global(object._displacement_jump_global),
    _interface_displacement_jump(object._interface_displacement_jump),
    _czm_total_rotation(object._czm_total_rotation)
{
}

void
KokkosSCZMComputeDisplacementJumpSmallStrain::computeProperties()
{
  if (!_sbm_distance_uo.kokkosSurrogateInterfaceMappingReady(boundaryIDs()))
    mooseError("Surrogate-interface mapping for '",
               name(),
               "' is unavailable. Ensure its 'sbm_distance_uo' executes before material "
               "evaluation.");
  InterfaceMaterial::computeProperties();
}

void
KokkosSCZMComputeDisplacementJumpSmallStrain::initializeSurrogateInterfaceMapping()
{
  const auto & mapping = _sbm_distance_uo.registerKokkosSurrogateInterfaceMapping(
      boundaryIDs(), kokkosInterfaceFaces());
  _surrogate_distance_vectors = mapping.surrogate_distance_vectors;
  _rotations = mapping.rotations;
}
