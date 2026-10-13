//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSCZMComputeLinearElasticBulkStress.h"

#include "FEProblem.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp",
                                KokkosSCZMComputeLinearElasticBulkStress);

InputParameters
KokkosSCZMComputeLinearElasticBulkStress::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addRequiredCoupledVar("displacements", "Displacement variables for the problem.");
  params.addRequiredParam<Real>("youngs_modulus", "Primary side Young's modulus.");
  params.addRequiredParam<Real>("poissons_ratio", "Primary side Poisson's ratio.");
  params.addParam<Real>("neighbor_youngs_modulus",
                        "Neighbor side Young's modulus. Defaults to 'youngs_modulus'.");
  params.addParam<Real>("neighbor_poissons_ratio",
                        "Neighbor side Poisson's ratio. Defaults to 'poissons_ratio'.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addClassDescription(
      "Computes the isotropic linear-elastic bulk stress and Lame constants on both physical "
      "sides of a shifted cohesive-zone interface. This material supplies the bulk response "
      "consumed by the Kokkos SCZM directional correction.");
  return params;
}

KokkosSCZMComputeLinearElasticBulkStress::KokkosSCZMComputeLinearElasticBulkStress(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ndisp(coupledComponents("displacements")),
    _youngs_modulus(getParam<Real>("youngs_modulus")),
    _poissons_ratio(getParam<Real>("poissons_ratio")),
    _neighbor_youngs_modulus(isParamSetByUser("neighbor_youngs_modulus")
                                 ? getParam<Real>("neighbor_youngs_modulus")
                                 : _youngs_modulus),
    _neighbor_poissons_ratio(isParamSetByUser("neighbor_poissons_ratio")
                                 ? getParam<Real>("neighbor_poissons_ratio")
                                 : _poissons_ratio),
    _displacement_gradients(kokkosCoupledGradients("displacements"))
{
  if (_ndisp != _mesh.dimension())
    paramError("displacements", "Number of displacements must match problem dimension.");
  if (_ndisp < 1 || _ndisp > 3)
    paramError("displacements", "One, two, or three displacement variables are required.");

  // The directional projection is coordinate-system independent, but the constitutive strain of
  // this compact producer is not: sym(grad_u) omits the hoop terms required in curvilinear
  // coordinates. Reject rather than silently produce a wrong bulk stress.
  if (_fe_problem.coordTransform().coordinateSystem() != Moose::COORD_XYZ)
    mooseError("KokkosSCZMComputeLinearElasticBulkStress '",
               name(),
               "' currently supports only the CARTESIAN coordinate system. In RZ or RSPHERICAL the "
               "bulk strain must include hoop terms, which this material does not compute.");

  if (_youngs_modulus <= 0.0)
    paramError("youngs_modulus", "Must be positive, but is ", _youngs_modulus, ".");
  if (_poissons_ratio <= -1.0 || _poissons_ratio >= 0.5)
    paramError("poissons_ratio", "Must satisfy -1 < nu < 0.5, but is ", _poissons_ratio, ".");
  if (_neighbor_youngs_modulus <= 0.0)
    paramError(
        "neighbor_youngs_modulus", "Must be positive, but is ", _neighbor_youngs_modulus, ".");
  if (_neighbor_poissons_ratio <= -1.0 || _neighbor_poissons_ratio >= 0.5)
    paramError("neighbor_poissons_ratio",
               "Must satisfy -1 < nu < 0.5, but is ",
               _neighbor_poissons_ratio,
               ".");

  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _sczm_bulk_stress = declareKokkosPropertyByName<Real, 2>(prefix + "sczm_bulk_stress", {3, 3});
  _sczm_bulk_stress_neighbor =
      declareKokkosPropertyByName<Real, 2>(prefix + "sczm_bulk_stress_neighbor", {3, 3});
  _sczm_bulk_lame = declareKokkosPropertyByName<Real, 1>(prefix + "sczm_bulk_lame", {2});
  _sczm_bulk_lame_neighbor =
      declareKokkosPropertyByName<Real, 1>(prefix + "sczm_bulk_lame_neighbor", {2});
}

KokkosSCZMComputeLinearElasticBulkStress::KokkosSCZMComputeLinearElasticBulkStress(
    const KokkosSCZMComputeLinearElasticBulkStress & object)
  : InterfaceMaterial(object),
    _ndisp(object._ndisp),
    _youngs_modulus(object._youngs_modulus),
    _poissons_ratio(object._poissons_ratio),
    _neighbor_youngs_modulus(object._neighbor_youngs_modulus),
    _neighbor_poissons_ratio(object._neighbor_poissons_ratio),
    _displacement_gradients(object._displacement_gradients),
    _sczm_bulk_stress(object._sczm_bulk_stress),
    _sczm_bulk_stress_neighbor(object._sczm_bulk_stress_neighbor),
    _sczm_bulk_lame(object._sczm_bulk_lame),
    _sczm_bulk_lame_neighbor(object._sczm_bulk_lame_neighbor)
{
}
