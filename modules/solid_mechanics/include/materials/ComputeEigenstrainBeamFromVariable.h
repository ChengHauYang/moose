//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeEigenstrainBeamBase.h"

/**
 * ComputeEigenstrainBeamFromVariable computes an eigenstrain from displacement and rotational
 * eigenstrain variables
 */
template <bool is_ad>
class ComputeEigenstrainBeamFromVariableTempl : public ComputeEigenstrainBeamBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeEigenstrainBeamFromVariableTempl(const InputParameters & parameters);

protected:
  void computeQpEigenstrain() override;

  /// Number of displacement eigenstrain variables
  const unsigned int _ndisp;

  /// Number of rotational eigenstrain variables
  const unsigned int _nrot;

  /// Displacemenet eigenstrain variable values
  const std::vector<const GenericVariableValue<is_ad> *> _disp;

  /// Rotational eigenstrain variable values
  const std::vector<const GenericVariableValue<is_ad> *> _rot;

  usingMaterialMembers;
  using ComputeEigenstrainBeamBaseTempl<is_ad>::_disp_eigenstrain;
  using ComputeEigenstrainBeamBaseTempl<is_ad>::_rot_eigenstrain;
};

using ComputeEigenstrainBeamFromVariable = ComputeEigenstrainBeamFromVariableTempl<false>;
using ADComputeEigenstrainBeamFromVariable = ComputeEigenstrainBeamFromVariableTempl<true>;