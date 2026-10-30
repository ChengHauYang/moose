//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "CZMComputeGlobalTractionTotalLagrangian.h"
#include "ADCZMComputeGlobalTractionTotalLagrangian.h"

class BoundaryShortestDistanceToSurface;

/** Computes total-Lagrangian SCZM PK1 traction using the true reference normal. */
class SCZMComputeGlobalTractionTotalLagrangian : public CZMComputeGlobalTractionTotalLagrangian
{
public:
  static InputParameters validParams();
  SCZMComputeGlobalTractionTotalLagrangian(const InputParameters & parameters);

protected:
  RealVectorValue referenceNormal() const override;

  const bool _shifted;
  const BoundaryShortestDistanceToSurface * _sbm_distance_uo;
};

/** AD counterpart of SCZMComputeGlobalTractionTotalLagrangian. */
class ADSCZMComputeGlobalTractionTotalLagrangian : public ADCZMComputeGlobalTractionTotalLagrangian
{
public:
  static InputParameters validParams();
  ADSCZMComputeGlobalTractionTotalLagrangian(const InputParameters & parameters);

protected:
  RealVectorValue referenceNormal() const override;

  const bool _shifted;
  const BoundaryShortestDistanceToSurface * _sbm_distance_uo;
};
