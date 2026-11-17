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

template <bool is_ad>
class ComputeSurfaceTensionKKSTempl : public Material
{
public:
  static InputParameters validParams();

  ComputeSurfaceTensionKKSTempl(const InputParameters & parameters);

protected:
  virtual void computeQpProperties() override;

  /// Order parameter
  const GenericVariableValue<is_ad> & _v;
  /// Gradient of order parameter
  const GenericVariableGradient<is_ad> & _grad_v;

  /// Material property for gradient energy coefficient
  const GenericMaterialProperty<Real, is_ad> & _kappa;
  /// Material property for barrier function
  const GenericMaterialProperty<Real, is_ad> & _g;
  /// double well height parameter
  Real _w;

  GenericMaterialProperty<RankTwoTensor, is_ad> & _planar_stress;
};

using ComputeSurfaceTensionKKS = ComputeSurfaceTensionKKSTempl<false>;
using ADComputeSurfaceTensionKKS = ComputeSurfaceTensionKKSTempl<true>;
