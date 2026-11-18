//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeExtraStressVDWGas.h"

registerMooseObject("SolidMechanicsApp", ComputeExtraStressVDWGas);
registerMooseObject("SolidMechanicsApp", ADComputeExtraStressVDWGas);

template <bool is_ad>
InputParameters
ComputeExtraStressVDWGasTempl<is_ad>::validParams()
{
  InputParameters params = ExtraStressBase::validParams();
  params.addClassDescription(
      "Computes a hydrostatic stress corresponding to the pressure of a van der Waals gas that is "
      "added as an extra_stress to the stress computed by the constitutive model");
  params.addRequiredParam<MaterialPropertyName>(
      "b", "Hard-sphere exclusion volume of van der Waals gas atoms in nm^3");
  params.addRequiredParam<MaterialPropertyName>("Va", "Atomic volume of lattice atoms in nm^3");
  params.addRequiredParam<MaterialPropertyName>("T", "Temperature in K");
  params.addRequiredCoupledVar("cg", "Gas concentration (relative to lattice atoms)");
  params.addParam<Real>("nondim_factor",
                        1.0,
                        "Optional factor to non-dimensionalize pressure (pressure is calculated in "
                        "Pa, set this factor to characteristic energy density used for "
                        "non-dimensionalization if desired)");
  return params;
}

template <bool is_ad>
ComputeExtraStressVDWGasTempl<is_ad>::ComputeExtraStressVDWGasTempl(
    const InputParameters & parameters)
  : ExtraStressBase(parameters),
    _b(this->template getGenericMaterialProperty<Real, is_ad>("b")),
    _Va(this->template getGenericMaterialProperty<Real, is_ad>("Va")),
    _T(this->template getGenericMaterialProperty<Real, is_ad>("T")),
    _cg(this->template coupledGenericValue<is_ad>("cg")),
    _nondim_factor(this->template getParam<Real>("nondim_factor")),
    _kB(1.38064852e-23) // Boltzmann constant in J/K
{
}

template <bool is_ad>
void
ComputeExtraStressVDWGasTempl<is_ad>::computeQpExtraStress()
{
  _extra_stress[_qp].zero();
  _extra_stress[_qp].addIa(-_kB * _T[_qp] / (_Va[_qp] / _cg[_qp] - _b[_qp]) * 1.0e27 /
                           _nondim_factor);
}

template class ComputeExtraStressVDWGasTempl<false>;
template class ComputeExtraStressVDWGasTempl<true>;
