//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GenericKernel.h"
#include "RankFourTensorForward.h"

/**
 * Adds the elasticity-tensor term for an asymptotic-expansion homogenization corrector equation.
 */
template <bool is_ad>
class AsymptoticExpansionHomogenizationKernelTempl : public GenericKernel<is_ad>
{
public:
  static InputParameters validParams();

  AsymptoticExpansionHomogenizationKernelTempl(const InputParameters & parameters);

protected:
  virtual GenericReal<is_ad> computeQpResidual() override;

  /// Base name of the material system that this kernel applies to
  const std::string _base_name;

  const GenericMaterialProperty<RankFourTensor, is_ad> & _elasticity_tensor;

private:
  /// An integer corresponding to the direction this kernel acts in
  const unsigned int _component;
  const unsigned int _column;
  const std::array<unsigned int, 6> _k_index;
  const std::array<unsigned int, 6> _l_index;
  const unsigned int _k;
  const unsigned int _l;

  usingGenericKernelMembers;
};

using AsymptoticExpansionHomogenizationKernel =
    AsymptoticExpansionHomogenizationKernelTempl<false>;
using ADAsymptoticExpansionHomogenizationKernel =
    AsymptoticExpansionHomogenizationKernelTempl<true>;
