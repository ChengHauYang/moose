//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "UserObject.h"
#include "MooseTypes.h"
#include "RankTwoTensor.h"
#include "RankFourTensor.h"

class GlobalStrainPeriodicDirectionInterface
{
public:
  virtual const VectorValue<bool> & getPeriodicDirections() const = 0;
};

/**
 * Interface for extracting the periodic directions, residual, and (for the
 * non-AD legacy path) Jacobian values from UserObjects associated with global
 * strain calculation.  The residual and Jacobian storage are generic so that
 * the AD path preserves derivatives.
 */
template <bool is_ad>
class GlobalStrainUserObjectInterfaceTempl : public GlobalStrainPeriodicDirectionInterface
{
public:
  virtual const GenericRankTwoTensor<is_ad> & getResidual() const = 0;
  virtual const GenericRankFourTensor<is_ad> & getJacobian() const = 0;
};

using GlobalStrainUserObjectInterface = GlobalStrainUserObjectInterfaceTempl<false>;
using ADGlobalStrainUserObjectInterface = GlobalStrainUserObjectInterfaceTempl<true>;
