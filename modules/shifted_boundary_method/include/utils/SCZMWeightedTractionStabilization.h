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
      "Dimensionless multiplier of the Robin parameter beta, which is derived once per paired "
      "interface quadrature point from the normal bulk stiffness over the element normal length on "
      "each side and then frozen.");
}

struct Settings
{
  bool enabled = false;
  Real gamma = 5.0;
};

inline Settings
settings(const MooseObject & object)
{
  Settings result;
  result.enabled = object.getParam<MooseEnum>("stabilization") == "weighted_traction";
  if (!result.enabled)
  {
    if (object.isParamSetByUser("stabilization_gamma"))
      object.paramError("stabilization_gamma",
                        "This parameter requires 'stabilization = weighted_traction'.");
    return result;
  }

  result.gamma = object.getParam<Real>("stabilization_gamma");
  return result;
}
} // namespace SCZMWeightedTractionStabilization
