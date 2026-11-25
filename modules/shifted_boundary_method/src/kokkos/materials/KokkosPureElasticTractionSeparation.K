//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosPureElasticTractionSeparation.h"
#include "KokkosSCZMStabilizationData.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp", KokkosPureElasticTractionSeparation);

InputParameters
KokkosPureElasticTractionSeparation::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addParam<MooseEnum>(
      "cohesive_law", MooseEnum("affine coupled_cubic", "affine"), "Native cohesive law.");
  params.addParam<Real>("normal_stiffness", "Stiffness in the normal direction.");
  params.addParam<Real>("tangent_stiffness", "Stiffness in tangential directions.");
  params.addParam<Real>("linear_stiffness", "Linear coefficient of the coupled-cubic law.");
  params.addParam<Real>("cubic_stiffness", "Cubic coefficient of the coupled-cubic law.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addClassDescription(
      "Computes a pure elastic cohesive traction-separation law with Kokkos.");
  return params;
}

KokkosPureElasticTractionSeparation::KokkosPureElasticTractionSeparation(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ndisp(_mesh.dimension()),
    _coupled_cubic(getParam<MooseEnum>("cohesive_law") == "coupled_cubic"),
    _normal_stiffness(isParamValid("normal_stiffness") ? getParam<Real>("normal_stiffness") : 0.0),
    _tangent_stiffness(isParamValid("tangent_stiffness") ? getParam<Real>("tangent_stiffness")
                                                         : 0.0),
    _cubic_linear_stiffness(isParamValid("linear_stiffness") ? getParam<Real>("linear_stiffness")
                                                             : 0.0),
    _cubic_stiffness(isParamValid("cubic_stiffness") ? getParam<Real>("cubic_stiffness") : 0.0),
    _interface_displacement_jump(getKokkosMaterialPropertyByName<Real, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "interface_displacement_jump"))
{
  if (_coupled_cubic)
  {
    if (!isParamValid("linear_stiffness"))
      paramError("linear_stiffness", "This parameter is required for the coupled_cubic law.");
    if (!isParamValid("cubic_stiffness"))
      paramError("cubic_stiffness", "This parameter is required for the coupled_cubic law.");
    if (_cubic_linear_stiffness < 0.0 || _cubic_stiffness < 0.0)
      paramError("cohesive_law", "The coupled_cubic coefficients must be nonnegative.");
  }
  else
  {
    if (!isParamValid("normal_stiffness"))
      paramError("normal_stiffness", "This parameter is required for the affine law.");
    if (!isParamValid("tangent_stiffness"))
      paramError("tangent_stiffness", "This parameter is required for the affine law.");
  }

  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _interface_traction =
      declareKokkosPropertyByName<Real, 1>(prefix + "interface_traction", {_ndisp});
  _dinterface_traction_djump =
      declareKokkosPropertyByName<Real, 2>(prefix + "dinterface_traction_djump", {_ndisp, _ndisp});
}

void
KokkosPureElasticTractionSeparation::robinPrepass(const KokkosSCZMStabilizationData & uo) const
{
  uo.robinPrepass(*this);
}

void
KokkosPureElasticTractionSeparation::checkStabilizationLaw(
    const KokkosSCZMStabilizationData & uo) const
{
  // The stabilization law parameters duplicate this material's law; they must agree
  if (uo.coupledCubicStabilization() != _coupled_cubic ||
      (_coupled_cubic && (uo.stabilizationLinearStiffness() != _cubic_linear_stiffness ||
                          uo.stabilizationCubicStiffness() != _cubic_stiffness)))
    mooseError("The weighted traction stabilization law of '",
               uo.name(),
               "' must match the cohesive law of '",
               name(),
               "'.");
}
