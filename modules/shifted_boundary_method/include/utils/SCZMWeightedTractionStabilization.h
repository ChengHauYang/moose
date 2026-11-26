//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "MooseObject.h"
#include "MooseEnum.h"

/** Configuration shared by the shifted cohesive-zone weighted-traction kernels. */
namespace SCZMWeightedTractionStabilization
{
/// Adds the stabilization selector, the only stabilization parameter of the host interface kernels
inline void
addModeParam(InputParameters & params)
{
  params.addParam<MooseEnum>(
      "stabilization",
      MooseEnum("none weighted_traction", "none"),
      "Interface flux stabilization. 'weighted_traction' solves a local Robin equation using the "
      "cohesive law and average bulk stress traction.");
}

inline void
addParams(InputParameters & params)
{
  addModeParam(params);
  params.addRangeCheckedParam<Real>(
      "stabilization_gamma",
      5.0,
      "stabilization_gamma > 0",
      "Dimensionless multiplier used to derive beta from the directional bulk tangent and element "
      "normal length.");
  params.addRangeCheckedParam<Real>(
      "stabilization_beta",
      "stabilization_beta > 0",
      "Optional fixed beta override. If omitted, beta is derived once per paired interface "
      "quadrature point and then frozen.");

  // Accepted so existing input files continue to parse. Generalized Robin stabilization obtains the
  // actual cohesive tangent from the constitutive evaluator and does not use these values.
  params.addRangeCheckedParam<Real>(
      "stabilization_normal_stiffness",
      "stabilization_normal_stiffness >= 0",
      "Deprecated reference normal cohesive stiffness retained for input compatibility.");
  params.addRangeCheckedParam<Real>(
      "stabilization_tangential_stiffness",
      "stabilization_tangential_stiffness >= 0",
      "Deprecated reference tangential cohesive stiffness retained for input compatibility.");
}

struct Settings
{
  bool enabled = false;
  bool beta_override = false;
  Real beta = 0.0;
  Real gamma = 5.0;
};

inline Settings
settings(const MooseObject & object)
{
  Settings result;
  result.enabled = object.getParam<MooseEnum>("stabilization") == "weighted_traction";
  if (!result.enabled)
  {
    for (const auto & name : {"stabilization_gamma",
                              "stabilization_beta",
                              "stabilization_normal_stiffness",
                              "stabilization_tangential_stiffness"})
      if (object.isParamSetByUser(name))
        object.paramError(name, "This parameter requires 'stabilization = weighted_traction'.");
    return result;
  }

  result.gamma = object.getParam<Real>("stabilization_gamma");
  result.beta_override = object.isParamValid("stabilization_beta");
  if (result.beta_override)
    result.beta = object.getParam<Real>("stabilization_beta");
  return result;
}
} // namespace SCZMWeightedTractionStabilization
