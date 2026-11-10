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

#include "RankTwoTensor.h"

/**
 * Computes an eigenstrain from a tensor-valued material property \f$\mathbf{A}\f$, a scalar
 * material-property prefactor \f$p\f$, and an optional constant offset tensor \f$\mathbf{B}\f$:
 * \f$\boldsymbol{\epsilon}^{eigen} = p\mathbf{A} + \mathbf{B}\f$.
 */
template <bool is_ad>
class ComputeVariableBaseEigenStrainTempl : public ComputeEigenstrainBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeVariableBaseEigenStrainTempl(const InputParameters & parameters);

protected:
  virtual void computeQpEigenstrain() override;

  const GenericMaterialProperty<RealTensorValue, is_ad> & _base_tensor;
  const GenericMaterialProperty<Real, is_ad> & _prefactor;
  RankTwoTensor _offset_tensor;

  using Material::_qp;
  using ComputeEigenstrainBaseTempl<is_ad>::_eigenstrain;
};

using ComputeVariableBaseEigenStrain = ComputeVariableBaseEigenStrainTempl<false>;
using ADComputeVariableBaseEigenStrain = ComputeVariableBaseEigenStrainTempl<true>;
