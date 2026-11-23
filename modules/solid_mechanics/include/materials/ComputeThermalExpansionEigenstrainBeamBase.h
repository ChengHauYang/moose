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
 * ComputeThermalExpansionEigenstrainBeamBase is a base class for all models that
 * compute beam eigenstrains due to thermal expansion of a material.
 */
template <bool is_ad>
class ComputeThermalExpansionEigenstrainBeamBaseTempl
  : public ComputeEigenstrainBeamBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeThermalExpansionEigenstrainBeamBaseTempl(const InputParameters & parameters);

protected:
  virtual void computeQpEigenstrain() override;
  /*
   * Compute the total thermal strain relative to the stress-free temperature at
   * the current temperature
   *
   * param thermal_strain    The current total linear thermal strain
   *                         (\delta L / L)
   */
  virtual GenericReal<is_ad> computeThermalStrain() = 0;

  /// Value of temperature at each quadrature point
  const GenericVariableValue<is_ad> & _temperature;

  /// Value of stress free temperature at each quadrature point
  const GenericVariableValue<is_ad> & _stress_free_temperature;

  /// Initial orientation of the beam
  RealGradient _initial_axis;

  usingMaterialMembers;
  using ComputeEigenstrainBeamBaseTempl<is_ad>::_disp_eigenstrain;
  using ComputeEigenstrainBeamBaseTempl<is_ad>::_rot_eigenstrain;
};

using ComputeThermalExpansionEigenstrainBeamBase =
    ComputeThermalExpansionEigenstrainBeamBaseTempl<false>;
using ADComputeThermalExpansionEigenstrainBeamBase =
    ComputeThermalExpansionEigenstrainBeamBaseTempl<true>;