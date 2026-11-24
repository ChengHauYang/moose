//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeEigenstrainBase.h"
#include "CompositeTensorBase.h"
#include "RankTwoTensor.h"

/**
 * CompositeEigenstrain provides a simple RankTwoTensor type
 * MaterialProperty that can be used as an Eigenstrain tensor in a mechanics simulation.
 * This tensor is computes as a weighted sum of base Eigenstrain tensors where each weight
 * can be a scalar material property that may depend on simulation variables.
 * The generic logic that computes a weighted sum of tensors is located in the
 * templated base class CompositeTensorBase.
 */
template <bool is_ad>
class CompositeEigenstrainTempl
  : public CompositeTensorBase<RankTwoTensor, ComputeEigenstrainBaseTempl<is_ad>, is_ad>
{
public:
  static InputParameters validParams();

  CompositeEigenstrainTempl(const InputParameters & parameters);

protected:
  virtual void computeQpEigenstrain() override;

  using CompositeTensorBase<RankTwoTensor, ComputeEigenstrainBaseTempl<is_ad>, is_ad>::_base_name;
  using CompositeTensorBase<RankTwoTensor, ComputeEigenstrainBaseTempl<is_ad>, is_ad>::_eigenstrain;
  using CompositeTensorBase<RankTwoTensor, ComputeEigenstrainBaseTempl<is_ad>, is_ad>::
      computeQpTensorProperties;
  using CompositeTensorBase<RankTwoTensor, ComputeEigenstrainBaseTempl<is_ad>, is_ad>::
      initializeDerivativeProperties;
};

using CompositeEigenstrain = CompositeEigenstrainTempl<false>;
using ADCompositeEigenstrain = CompositeEigenstrainTempl<true>;
