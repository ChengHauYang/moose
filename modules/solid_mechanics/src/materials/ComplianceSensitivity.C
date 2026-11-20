//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComplianceSensitivity.h"

registerMooseObject("SolidMechanicsApp", ComplianceSensitivity);
registerMooseObject("SolidMechanicsApp", ADComplianceSensitivity);

template <bool is_ad>
InputParameters
ComplianceSensitivityTempl<is_ad>::validParams()
{
  InputParameters params = StrainEnergyDensityTempl<is_ad>::validParams();
  params.addClassDescription("Computes compliance sensitivity needed for SIMP method.");
  params.addRequiredCoupledVar("design_density", "Design density variable name.");
  params.addRequiredParam<MaterialPropertyName>("youngs_modulus",
                                                "DerivativeParsedMaterial for Youngs modulus.");

  return params;
}

template <bool is_ad>
ComplianceSensitivityTempl<is_ad>::ComplianceSensitivityTempl(const InputParameters & parameters)
  : StrainEnergyDensityTempl<is_ad>(parameters),
    _sensitivity(this->template declareGenericProperty<Real, is_ad>(_base_name + "sensitivity")),
    _design_density(this->template coupledGenericValue<is_ad>("design_density")),
    _design_density_name(this->coupledName("design_density", 0)),
    _dEdp(this->template getGenericMaterialProperty<Real, is_ad>(this->derivativePropertyNameFirst(
        this->template getParam<MaterialPropertyName>("youngs_modulus"), _design_density_name))),
    _youngs_modulus(this->template getGenericMaterialProperty<Real, is_ad>("youngs_modulus"))
{
}

template <bool is_ad>
void
ComplianceSensitivityTempl<is_ad>::computeQpProperties()
{
  // Call the parent class's method to compute the strain energy density
  StrainEnergyDensityTempl<is_ad>::computeQpProperties();
  _sensitivity[_qp] = -_dEdp[_qp] * _strain_energy_density[_qp] / _youngs_modulus[_qp];
}

template class ComplianceSensitivityTempl<false>;
template class ComplianceSensitivityTempl<true>;
