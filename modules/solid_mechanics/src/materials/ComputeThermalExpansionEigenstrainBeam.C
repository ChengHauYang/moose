//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeThermalExpansionEigenstrainBeam.h"

registerMooseObject("SolidMechanicsApp", ComputeThermalExpansionEigenstrainBeam);
registerMooseObject("SolidMechanicsApp", ADComputeThermalExpansionEigenstrainBeam);

template <bool is_ad>
InputParameters
ComputeThermalExpansionEigenstrainBeamTempl<is_ad>::validParams()
{
  InputParameters params = ComputeThermalExpansionEigenstrainBeamBaseTempl<is_ad>::validParams();
  params.addClassDescription("Computes eigenstrain due to thermal expansion "
                             "with a constant coefficient");
  params.addParam<Real>("thermal_expansion_coeff", "Thermal expansion coefficient");

  return params;
}

template <bool is_ad>
ComputeThermalExpansionEigenstrainBeamTempl<is_ad>::ComputeThermalExpansionEigenstrainBeamTempl(
    const InputParameters & parameters)
  : ComputeThermalExpansionEigenstrainBeamBaseTempl<is_ad>(parameters),
    _thermal_expansion_coeff(this->template getParam<Real>("thermal_expansion_coeff"))
{
}

template <bool is_ad>
GenericReal<is_ad>
ComputeThermalExpansionEigenstrainBeamTempl<is_ad>::computeThermalStrain()
{
  return _thermal_expansion_coeff * (_temperature[_qp] - _stress_free_temperature[_qp]);
}

template class ComputeThermalExpansionEigenstrainBeamTempl<false>;
template class ComputeThermalExpansionEigenstrainBeamTempl<true>;