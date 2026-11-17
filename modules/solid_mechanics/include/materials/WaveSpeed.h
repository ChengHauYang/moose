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

/**
 * This material computes the wave speed for dynamic simulations using
 * the Young's modulus (or equivalent metric) and the density.
 * This object can be used, for example, in contact algorithms for
 * explicit dynamics.
 */
template <bool is_ad>
class WaveSpeedTempl : public Material
{
public:
  static InputParameters validParams();

  WaveSpeedTempl(const InputParameters & parameters);

protected:
  virtual void computeQpProperties();

private:
  /// The wave speed material generated here
  GenericMaterialProperty<Real, is_ad> & _wave_speed;

  /// Density of the material
  const GenericMaterialProperty<Real, is_ad> & _material_density;

  /// Effective stiffness of element: function of material properties and element size
  const GenericMaterialProperty<Real, is_ad> & _effective_stiffness;
};

using WaveSpeed = WaveSpeedTempl<false>;
using ADWaveSpeed = WaveSpeedTempl<true>;
