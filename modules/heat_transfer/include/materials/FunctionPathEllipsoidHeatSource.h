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
#include "SpatioTemporalPath.h"
#include "SpatioTemporalPathInterface.h"

class Function;

/**
 * Double ellipsoid heat source distribution.
 */
class FunctionPathEllipsoidHeatSource : public Material
{
public:
  static InputParameters validParams();

  FunctionPathEllipsoidHeatSource(const InputParameters & parameters);

protected:
  virtual void computeQpProperties() override;

  /// power
  const Function * _function_P;
  const Real _P;
  /// process efficienty
  const Real _eta;
  /// process efficiency function
  const Function * _function_efficiency;

  /// transverse ellipsoid axe
  const Function * _function_rx;
  /// depth ellipsoid axe
  const Function * _function_ry;
  /// longitudinal ellipsoid axe
  const Function * _function_rz;

  const Real _rx;
  const Real _ry;
  const Real _rz;

  /// scaling factor
  const Real _f;
  /// path of the heat source, x, y, z components
  const Function & _function_x;
  const Function & _function_y;
  const Function & _function_z;

  const Function * _function_weave_amp_x;
  const Function * _function_weave_amp_y;
  const Function * _function_weave_amp_z;

  const Function * _function_torch_speed;

  const Real _wavelength;

  ADMaterialProperty<Real> & _volumetric_heat;

  /// The path
  const SpatioTemporalPath * _path;
};
