//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeStrainBase.h"
#include "ADComputeStrainBase.h"

/**
 * ComputeCosseratSmallStrain defines Cosserat strain tensor, assuming small strains.
 */
template <bool is_ad>
class ComputeCosseratSmallStrainTempl
  : public std::conditional_t<is_ad, ADComputeStrainBase, ComputeStrainBase>
{
public:
  static InputParameters validParams();

  ComputeCosseratSmallStrainTempl(const InputParameters & parameters);

protected:
  virtual void computeQpProperties() override;

  /// The Cosserat curvature strain: curvature_ij = nabla_j CosseratRotation_i
  GenericMaterialProperty<RankTwoTensor, is_ad> & _curvature;

  /// The number of Cosserat rotation variables supplied by the user (must be 3)
  const unsigned int _nrots;

  /// The Cosserat rotations
  const std::vector<const GenericVariableValue<is_ad> *> _wc;

  /// Grad(Cosserat rotation)
  const std::vector<const GenericVariableGradient<is_ad> *> _grad_wc;

  using StrainBase = std::conditional_t<is_ad, ADComputeStrainBase, ComputeStrainBase>;
  using StrainBase::_eigenstrains;
  using StrainBase::_grad_disp;
  using StrainBase::_mechanical_strain;
  using StrainBase::_qp;
  using StrainBase::_total_strain;
};

using ComputeCosseratSmallStrain = ComputeCosseratSmallStrainTempl<false>;
using ADComputeCosseratSmallStrain = ComputeCosseratSmallStrainTempl<true>;
