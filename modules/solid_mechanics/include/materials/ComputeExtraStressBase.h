//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "Material.h"
#include "RankTwoTensor.h"

/**
 * ComputeExtraStressBaseTempl is the base class for extra_stress, which is added to stress
 * calculated by the material's constitutive model
 */
template <bool is_ad>
class ComputeExtraStressBaseTempl : public Material
{
public:
  static InputParameters validParams();

  ComputeExtraStressBaseTempl(const InputParameters & parameters);

protected:
  virtual void computeQpProperties();
  virtual void computeQpExtraStress() = 0;

  /// Base name of the material system
  const std::string _base_name;
  std::string _extra_stress_name;

  GenericMaterialProperty<RankTwoTensor, is_ad> & _extra_stress;
};

using ComputeExtraStressBase = ComputeExtraStressBaseTempl<false>;
using ADComputeExtraStressBase = ComputeExtraStressBaseTempl<true>;
