//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "VoceHardeningStressUpdate.h"

registerMooseObject("TensorMechanicsApp", VoceHardeningStressUpdate);

InputParameters
VoceHardeningStressUpdate::validParams()
{
  InputParameters params = ADIsotropicPlasticityStressUpdate::validParams();
  params.set<Real>("yield_stress") = 1.0;
  params.set<Real>("hardening_constant") = 1.0;

  params.addRequiredParam<MaterialPropertyName>("s0", "s0");
  params.addRequiredParam<MaterialPropertyName>("s1", "s1");
  params.addRequiredParam<MaterialPropertyName>("delta", "delta");

  return params;
}

VoceHardeningStressUpdate::VoceHardeningStressUpdate(const InputParameters & parameters)
  : ADIsotropicPlasticityStressUpdate(parameters),
    _s0(getADMaterialProperty<Real>("s0")),
    _s1(getADMaterialProperty<Real>("s1")),
    _delta(getADMaterialProperty<Real>("delta"))
{
}

void
VoceHardeningStressUpdate::computeStressInitialize(const ADReal & effectiveTrialStress,
                                                   const ADRankFourTensor & elasticity_tensor)
{
  ADRadialReturnStressUpdate::computeStressInitialize(effectiveTrialStress, elasticity_tensor);
  computeYieldStress(elasticity_tensor);

  _yield_condition = effectiveTrialStress - _hardening_variable_old[_qp] - _yield_stress;
  _hardening_variable[_qp] = _hardening_variable_old[_qp];
  _plastic_strain[_qp] = _plastic_strain_old[_qp];
}

ADReal
VoceHardeningStressUpdate::computeHardeningValue(const ADReal & scalar)
{
  return (_s1[_qp] - _s0[_qp]) * (1.0 - std::exp(-_delta[_qp] * scalar));
}

ADReal
VoceHardeningStressUpdate::computeHardeningDerivative(const ADReal & scalar)
{
  return (_s1[_qp] - _s0[_qp]) * _delta[_qp] * std::exp(-_delta[_qp] * scalar);
}

void
VoceHardeningStressUpdate::computeYieldStress(const ADRankFourTensor & /*elasticity_tensor*/)
{
  _yield_stress = _s0[_qp];
}
