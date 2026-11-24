//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#ifdef NEML2_ENABLED

// MOOSE includes
#include "TorchPreKernel.h"

class MooseVariableScalar;

class TorchSmallStrain : public TorchPreKernel
{
public:
  static InputParameters validParams();

  TorchSmallStrain(const InputParameters & parameters);

protected:
  /// Calculate small strain from displacement gradients
  void forward() override;

  /// Displacement gradients
  ///@{
  const at::Tensor * _grad_disp_x;
  const at::Tensor * _grad_disp_y;
  const at::Tensor * _grad_disp_z;
  ///@}

  /// Whether to apply the additive (B-bar) volumetric locking correction
  const bool _stabilize_strain;
  /// Macro gradient scalar variable added after the volumetric correction, if any
  const MooseVariableScalar * const _macro_gradient;
  /// Tensor indices of the macro gradient components
  const std::vector<std::pair<unsigned int, unsigned int>> _macro_gradient_indices;
};

#endif // NEML2_ENABLED
