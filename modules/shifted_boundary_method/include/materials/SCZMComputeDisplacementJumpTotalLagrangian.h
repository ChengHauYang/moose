//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "SCZMComputeDisplacementJumpBase.h"

/**
 * Computes the shifted cohesive displacement jump using total-Lagrangian kinematics.
 */
template <bool is_ad>
class SCZMComputeDisplacementJumpTotalLagrangianTempl
  : public SCZMComputeDisplacementJumpBase<is_ad>
{
public:
  static InputParameters validParams();
  SCZMComputeDisplacementJumpTotalLagrangianTempl(const InputParameters & parameters);

protected:
  void initQpStatefulProperties() override;
  void computeLocalDisplacementJump() override;
  void computeRotationMatrices() override;

  /// Computes the surrogate-interface deformation gradient and its polar rotation.
  void computeFandR();

  GenericMaterialProperty<RankTwoTensor, is_ad> & _F;
  GenericMaterialProperty<RankTwoTensor, is_ad> & _R;
  GenericMaterialProperty<RankTwoTensor, is_ad> & _czm_reference_rotation;

  usingSCZMComputeDisplacementJumpBaseMembers;
};

using SCZMComputeDisplacementJumpTotalLagrangian =
    SCZMComputeDisplacementJumpTotalLagrangianTempl<false>;
using ADSCZMComputeDisplacementJumpTotalLagrangian =
    SCZMComputeDisplacementJumpTotalLagrangianTempl<true>;
