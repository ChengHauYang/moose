//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SCZMComputeGlobalTractionTotalLagrangian.h"
#include "BoundaryShortestDistanceToSurface.h"

registerMooseObject("ShiftedBoundaryMethodApp", SCZMComputeGlobalTractionTotalLagrangian);
registerMooseObject("ShiftedBoundaryMethodApp", ADSCZMComputeGlobalTractionTotalLagrangian);

namespace
{
InputParameters
validParamsImpl(InputParameters params)
{
  params.addParam<bool>("no_shifted", false, "Disable shifted integration corrections.");
  params.addParam<UserObjectName>("sbm_distance_uo",
                                  "UserObject that provides the true-interface reference normal.");
  params.addClassDescription(
      "Computes total-Lagrangian shifted cohesive PK1 traction using the true reference normal.");
  return params;
}
}

InputParameters
SCZMComputeGlobalTractionTotalLagrangian::validParams()
{
  return validParamsImpl(CZMComputeGlobalTractionTotalLagrangian::validParams());
}

SCZMComputeGlobalTractionTotalLagrangian::SCZMComputeGlobalTractionTotalLagrangian(
    const InputParameters & parameters)
  : CZMComputeGlobalTractionTotalLagrangian(parameters),
    _shifted(!getParam<bool>("no_shifted")),
    _sbm_distance_uo(_shifted ? &getUserObject<BoundaryShortestDistanceToSurface>("sbm_distance_uo")
                              : nullptr)
{
}

RealVectorValue
SCZMComputeGlobalTractionTotalLagrangian::referenceNormal() const
{
  // For SCZM, _normals is the surrogate face normal. Nanson mapping instead uses the true
  // reference normal at the projected interface point.
  return _shifted ? RealVectorValue(_sbm_distance_uo->trueNormal(
                        std::make_pair(_current_elem->id(), _current_side), _qp))
                  : RealVectorValue(_normals[_qp]);
}

InputParameters
ADSCZMComputeGlobalTractionTotalLagrangian::validParams()
{
  return validParamsImpl(ADCZMComputeGlobalTractionTotalLagrangian::validParams());
}

ADSCZMComputeGlobalTractionTotalLagrangian::ADSCZMComputeGlobalTractionTotalLagrangian(
    const InputParameters & parameters)
  : ADCZMComputeGlobalTractionTotalLagrangian(parameters),
    _shifted(!getParam<bool>("no_shifted")),
    _sbm_distance_uo(_shifted ? &getUserObject<BoundaryShortestDistanceToSurface>("sbm_distance_uo")
                              : nullptr)
{
}

RealVectorValue
ADSCZMComputeGlobalTractionTotalLagrangian::referenceNormal() const
{
  // For SCZM, _normals is the surrogate face normal. Nanson mapping instead uses the true
  // reference normal at the projected interface point.
  return _shifted ? RealVectorValue(_sbm_distance_uo->trueNormal(
                        std::make_pair(_current_elem->id(), _current_side), _qp))
                  : RealVectorValue(_normals[_qp]);
}
