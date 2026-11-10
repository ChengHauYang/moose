//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeRotatedElasticityTensorBase.h"

/**
 * Computes a concentration-dependent elasticity tensor using \f$\mathbf{C}(c) =
 * \mathbf{C}_0 + c(\mathbf{C}_1 - \mathbf{C}_0)\f$. The endpoints may be rotated before
 * interpolation to represent material orientations.
 */
template <bool is_ad>
class ComputeConcentrationDependentElasticityTensorTempl
    : public ComputeRotatedElasticityTensorBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeConcentrationDependentElasticityTensorTempl(const InputParameters & parameters);

protected:
  virtual void computeQpElasticityTensor() override;

  /// Elasticity tensor for phase with zero concentration.
  RankFourTensor _Cijkl0;
  /// Elasticity tensor for phase with concentration 1.
  RankFourTensor _Cijkl1;
  /// Concentration variable.
  const GenericVariableValue<is_ad> & _c;
  VariableName _c_name;

  /// Derivative of elasticity tensor with respect to concentration.
  MaterialProperty<RankFourTensor> * _delasticity_tensor_dc;

  using ComputeRotatedElasticityTensorBaseTempl<is_ad>::_Euler_angles;
  using ComputeElasticityTensorBaseTempl<is_ad>::_elasticity_tensor;
  using ComputeElasticityTensorBaseTempl<is_ad>::_elasticity_tensor_name;
  using Material::_qp;
};

using ComputeConcentrationDependentElasticityTensor =
    ComputeConcentrationDependentElasticityTensorTempl<false>;
using ADComputeConcentrationDependentElasticityTensor =
    ComputeConcentrationDependentElasticityTensorTempl<true>;
