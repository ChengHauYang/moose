//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "Material.h"
#include "RankTwoTensorForward.h"

/**
 * Calculates an Extra-Stress tensor that lies in the plane of an interface
 * defined by the gradient of an order parameter.
 */
template <bool is_ad>
class ComputeInterfaceStressTempl : public Material
{
public:
  static InputParameters validParams();

  ComputeInterfaceStressTempl(const InputParameters & parameters);

protected:
  virtual void computeQpProperties() override;

  std::size_t _nvar;
  std::vector<const GenericVariableGradient<is_ad> *> _grad_v;
  std::vector<Real> _op_range;
  std::vector<Real> _stress;

  GenericMaterialProperty<RankTwoTensor, is_ad> & _planar_stress;
};

using ComputeInterfaceStress = ComputeInterfaceStressTempl<false>;
using ADComputeInterfaceStress = ComputeInterfaceStressTempl<true>;
