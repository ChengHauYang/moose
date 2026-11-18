//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeExtraStressBase.h"

/**
 * Computes a constant extra stress that is added to the stress calculated
 * by the constitutive model
 */
template <bool is_ad>
class ComputeExtraStressConstantTempl : public ComputeExtraStressBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeExtraStressConstantTempl(const InputParameters & parameters);

protected:
  virtual void computeQpExtraStress();

  const GenericMaterialProperty<Real, is_ad> & _prefactor;

  RankTwoTensor _extra_stress_tensor;

  using ExtraStressBase = ComputeExtraStressBaseTempl<is_ad>;
  using ExtraStressBase::_extra_stress;
  using ExtraStressBase::_qp;
};

using ComputeExtraStressConstant = ComputeExtraStressConstantTempl<false>;
using ADComputeExtraStressConstant = ComputeExtraStressConstantTempl<true>;
