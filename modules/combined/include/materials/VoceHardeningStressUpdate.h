//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "IsotropicPlasticityStressUpdate.h"

class VoceHardeningStressUpdate : public ADIsotropicPlasticityStressUpdate
{
public:
  static InputParameters validParams();

  VoceHardeningStressUpdate(const InputParameters & parameters);

protected:
  virtual void computeStressInitialize(const ADReal & effectiveTrialStress,
                                       const ADRankFourTensor & elasticity_tensor) override;

  virtual void computeYieldStress(const ADRankFourTensor & elasticity_tensor) override;
  virtual ADReal computeHardeningValue(const ADReal & scalar) override;
  virtual ADReal computeHardeningDerivative(const ADReal & scalar) override;

  const ADMaterialProperty<Real> & _s0;
  const ADMaterialProperty<Real> & _s1;
  const ADMaterialProperty<Real> & _delta;
};
