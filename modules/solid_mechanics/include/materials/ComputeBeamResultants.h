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
#include "RankTwoTensor.h"

/**
 * Computes forces and moments using elasticity
 */
template <bool is_ad>
class ComputeBeamResultantsTempl : public Material
{
public:
  static InputParameters validParams();

  ComputeBeamResultantsTempl(const InputParameters & parameters);

protected:
  virtual void initQpStatefulProperties() override;
  virtual void computeQpProperties() override;

  /// Mechanical displacement strain increment in beam local coordinate system
  const GenericMaterialProperty<RealVectorValue, is_ad> & _disp_strain_increment;

  /// Mechanical rotational strain increment in beam local coordinate system
  const GenericMaterialProperty<RealVectorValue, is_ad> & _rot_strain_increment;

  /// Material stiffness vector that relates displacement strain increment to force increment
  const MaterialProperty<RealVectorValue> & _material_stiffness;

  /// Material flexure vector that relates rotational strain increment to moment increment
  const MaterialProperty<RealVectorValue> & _material_flexure;

  /// Rotational transformation from global to current beam local coordinate system
  const MaterialProperty<RankTwoTensor> & _total_rotation;

  /// Current force vector in global coordinate system
  GenericMaterialProperty<RealVectorValue, is_ad> & _force;

  /// Current moment vector in global coordinate system
  GenericMaterialProperty<RealVectorValue, is_ad> & _moment;

  /// Old force vector in global coordinate system
  const MaterialProperty<RealVectorValue> & _force_old;

  /// Old force vector in global coordinate system
  const MaterialProperty<RealVectorValue> & _moment_old;

  usingMaterialMembers;
};

using ComputeBeamResultants = ComputeBeamResultantsTempl<false>;
using ADComputeBeamResultants = ComputeBeamResultantsTempl<true>;