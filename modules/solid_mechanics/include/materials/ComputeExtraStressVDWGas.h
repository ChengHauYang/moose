//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeExtraStressBase.h"

/**
 * Computes a concentration-dependent ExtraStress bases on the van der Waals
 * equation of state that is added to the stress computed by the constitutive model
 */
template <bool is_ad>
class ComputeExtraStressVDWGasTempl : public ComputeExtraStressBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeExtraStressVDWGasTempl(const InputParameters & parameters);

protected:
  virtual void computeQpExtraStress();

  const GenericMaterialProperty<Real, is_ad> & _b;
  const GenericMaterialProperty<Real, is_ad> & _Va;
  const GenericMaterialProperty<Real, is_ad> & _T;

  const GenericVariableValue<is_ad> & _cg;
  const Real _nondim_factor;
  const Real _kB;

  using ExtraStressBase = ComputeExtraStressBaseTempl<is_ad>;
  using ExtraStressBase::_extra_stress;
  using ExtraStressBase::_qp;
};

using ComputeExtraStressVDWGas = ComputeExtraStressVDWGasTempl<false>;
using ADComputeExtraStressVDWGas = ComputeExtraStressVDWGasTempl<true>;
