//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeCosseratStressBase.h"

/**
 * Computes Cosserat stress and couple-stress following linear elasticity theory.
 */
template <bool is_ad>
class ComputeCosseratLinearElasticStressTempl : public ComputeCosseratStressBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeCosseratLinearElasticStressTempl(const InputParameters & parameters);

  virtual void initialSetup() override;

protected:
  virtual void computeQpStress() override;

  /// Name of the elasticity tensor material property
  const std::string _elasticity_tensor_name;
  /// Elasticity tensor material property
  const MaterialProperty<RankFourTensor> & _elasticity_tensor;

  using ComputeCosseratStressBaseTempl<is_ad>::_base_name;
  using ComputeCosseratStressBaseTempl<is_ad>::_curvature;
  using ComputeCosseratStressBaseTempl<is_ad>::_elastic_flexural_rigidity_tensor;
  using ComputeCosseratStressBaseTempl<is_ad>::_elastic_strain;
  using ComputeCosseratStressBaseTempl<is_ad>::_Jacobian_mult_couple;
  using ComputeCosseratStressBaseTempl<is_ad>::_mechanical_strain;
  using ComputeCosseratStressBaseTempl<is_ad>::_qp;
  using ComputeCosseratStressBaseTempl<is_ad>::_stress;
  using ComputeCosseratStressBaseTempl<is_ad>::_stress_couple;
};

using ComputeCosseratLinearElasticStress = ComputeCosseratLinearElasticStressTempl<false>;
using ADComputeCosseratLinearElasticStress = ComputeCosseratLinearElasticStressTempl<true>;
