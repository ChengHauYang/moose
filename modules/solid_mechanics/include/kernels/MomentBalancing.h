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
#include "RankTwoTensorForward.h"
#include "RankFourTensorForward.h"

/**
 * This Kernel computes
 * epsilon_ijk * stress_jk (sum over j and k)
 * "i" is called _component in this class
 * and epsilon is the permutation pseudo-tensor
 *
 * This Kernel is added to CosseratStressDivergenceTensors
 * to form the equilibrium equations for the
 * Cosserat moment-stress.
 */
template <bool is_ad>
class MomentBalancingTempl : public GenericKernel<is_ad>
{
public:
  static InputParameters validParams();

  MomentBalancingTempl(const InputParameters & parameters);

protected:
  virtual GenericReal<is_ad> computeQpResidual() override;

  virtual Real computeQpJacobian() override;

  virtual Real computeQpOffDiagJacobian(unsigned int jvar) override;

  /// the stress tensor (not the moment stress) at the quad-point.
  const GenericMaterialProperty<RankTwoTensor, is_ad> & _stress;

  /**
   * d(stress tensor)/(d strain tensor)
   * Here strain_ij = grad_j disp_i + epsilon_ijk * wc_k
   */
  const MaterialProperty<RankFourTensor> * const _Jacobian_mult;

  /// The Kernel computes epsilon_{component j k}*stress_{j k}
  const unsigned int _component;

  // number of Cosserat rotation components provided by the user
  const unsigned int _nrots;

  /// the moose variable numbers for the Cosserat rotation degrees of freedom
  std::vector<unsigned int> _wc_var;

  // number of displacement components provided by the user
  const unsigned int _ndisp;

  /// the moose variable numbers for the displacements
  std::vector<unsigned int> _disp_var;

  using GenericKernel<is_ad>::_i;
  using GenericKernel<is_ad>::_qp;
  using GenericKernel<is_ad>::_test;
  using GenericKernel<is_ad>::_phi;
  using GenericKernel<is_ad>::_grad_phi;
  using GenericKernel<is_ad>::_var;
  using GenericKernel<is_ad>::_mesh;
};

using MomentBalancing = MomentBalancingTempl<false>;
using ADMomentBalancing = MomentBalancingTempl<true>;
