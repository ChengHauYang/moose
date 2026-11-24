//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ScalarKernel.h"
#include "ADScalarKernel.h"
#include "MooseTypes.h"
#include "RankTwoTensorForward.h"
#include "RankFourTensorForward.h"

#include "GlobalStrainUserObjectInterface.h"

template <bool is_ad>
class GlobalStrainTempl : public std::conditional_t<is_ad, ADScalarKernel, ScalarKernel>
{
public:
  static InputParameters validParams();

  GlobalStrainTempl(const InputParameters & parameters);

  virtual void reinit() override;
  virtual void computeResidual() override;
  virtual void computeJacobian() override;
  virtual void computeOffDiagJacobian(unsigned int jvar) override;

protected:
  virtual GenericReal<is_ad> computeQpResidual() override;
  virtual void assignComponentIndices(Order var_order);

  using Parent = std::conditional_t<is_ad, ADScalarKernel, ScalarKernel>;

  const GlobalStrainUserObjectInterfaceTempl<is_ad> & _pst;
  const GenericRankTwoTensor<is_ad> & _pst_residual;
  const VectorValue<bool> & _periodic_dir;

  std::vector<std::pair<unsigned int, unsigned int>> _components;
  const unsigned int _dim;
};

using GlobalStrain = GlobalStrainTempl<false>;
using ADGlobalStrain = GlobalStrainTempl<true>;
