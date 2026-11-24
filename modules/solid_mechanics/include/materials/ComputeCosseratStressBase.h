//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeStressBase.h"
#include "ADComputeStressBase.h"

/**
 * ComputeCosseratStressBaseTempl is the base class for Cosserat stress tensors.
 */
template <bool is_ad>
class ComputeCosseratStressBaseTempl
  : public std::conditional_t<is_ad, ADComputeStressBase, ComputeStressBase>
{
public:
  static InputParameters validParams();

  ComputeCosseratStressBaseTempl(const InputParameters & parameters);

protected:
  virtual void initQpStatefulProperties() override;
  virtual void computeQpStress() override = 0;

  /// The Cosserat curvature strain
  const GenericMaterialProperty<RankTwoTensor, is_ad> & _curvature;

  /// The Cosserat elastic flexural rigidity tensor
  const MaterialProperty<RankFourTensor> & _elastic_flexural_rigidity_tensor;

  /// The Cosserat couple-stress
  GenericMaterialProperty<RankTwoTensor, is_ad> & _stress_couple;

  /// Derivative of couple-stress with respect to curvature
  MaterialProperty<RankFourTensor> * const _Jacobian_mult_couple;

  using StressBase = std::conditional_t<is_ad, ADComputeStressBase, ComputeStressBase>;
  using StressBase::_qp;
};

using ComputeCosseratStressBase = ComputeCosseratStressBaseTempl<false>;
using ADComputeCosseratStressBase = ComputeCosseratStressBaseTempl<true>;
