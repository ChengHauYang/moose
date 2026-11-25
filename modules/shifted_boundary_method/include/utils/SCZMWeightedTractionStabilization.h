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
inline void
addParams(InputParameters & params)
{
  params.addParam<MooseEnum>(
      "stabilization",
      MooseEnum("none weighted_traction", "none"),
      "Interface flux stabilization. 'weighted_traction' solves a local Robin equation using the "
      "cohesive law and average bulk stress traction.");
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

/** Legacy fixed weights used only by the host SCZMInterfaceKernelBase. */
struct Weights
{
  bool enabled = false;
  Real normal = 0.0;
  Real tangential = 0.0;
};

inline Weights
weights(const MooseObject & object)
{
  const auto configuration = settings(object);
  Weights result;
  result.enabled = configuration.enabled;
  if (!result.enabled)
    return result;

  if (!configuration.beta_override || !object.isParamValid("stabilization_normal_stiffness"))
    object.paramError(
        "stabilization",
        "Generalized weighted traction without reference cohesive stiffness is supported only by "
        "the Kokkos kernels. The host kernels still require 'stabilization_beta' and "
        "'stabilization_normal_stiffness'.");

  const auto normal_stiffness = object.getParam<Real>("stabilization_normal_stiffness");
  const auto tangential_stiffness =
      object.isParamValid("stabilization_tangential_stiffness")
          ? object.getParam<Real>("stabilization_tangential_stiffness")
          : normal_stiffness;
  result.normal = configuration.beta / (normal_stiffness + configuration.beta);
  result.tangential = configuration.beta / (tangential_stiffness + configuration.beta);
  return result;
}
} // namespace SCZMWeightedTractionStabilization
