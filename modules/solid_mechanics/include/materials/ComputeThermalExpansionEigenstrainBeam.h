//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeThermalExpansionEigenstrainBeamBase.h"

/**
 * Computes eigenstrain due to thermal expansion with a constant coefficient
 */
template <bool is_ad>
class ComputeThermalExpansionEigenstrainBeamTempl
  : public ComputeThermalExpansionEigenstrainBeamBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeThermalExpansionEigenstrainBeamTempl(const InputParameters & parameters);

protected:
  virtual GenericReal<is_ad> computeThermalStrain() override;

  /// Constant thermal expansion coefficient
  const Real _thermal_expansion_coeff;

  usingMaterialMembers;
  using ComputeThermalExpansionEigenstrainBeamBaseTempl<is_ad>::_temperature;
  using ComputeThermalExpansionEigenstrainBeamBaseTempl<is_ad>::_stress_free_temperature;
};

using ComputeThermalExpansionEigenstrainBeam = ComputeThermalExpansionEigenstrainBeamTempl<false>;
using ADComputeThermalExpansionEigenstrainBeam = ComputeThermalExpansionEigenstrainBeamTempl<true>;