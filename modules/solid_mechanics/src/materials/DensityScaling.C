//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityScaling.h"
#include "libmesh/utility.h"

registerMooseObject("SolidMechanicsApp", DensityScaling);
registerMooseObject("SolidMechanicsApp", ADDensityScaling);

template <bool is_ad>
InputParameters
DensityScalingTempl<is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription(
      "Computes the scaled inertial density needed to enable stable explicit time-stepping using "
      "the "
      "desired_time_step in solid-mechanics problems.  Note that if this inertial density is used "
      "in input files (for instance, in the mass matrix) it will impact the dynamics of the "
      "system, largely eliminating high-frequency oscillations, and impacting low-frequency "
      "dynamics.  Hence, use with caution.");
  params.addRequiredParam<MaterialPropertyName>(
      "true_density",
      "Name of Material Property defining the true inertial density of the material.");
  params.addRequiredParam<MaterialPropertyName>(
      "scaled_density", "Name of the scaled density property that this Material computes.");
  params.addParam<MaterialPropertyName>(
      "additional_density",
      "additional_density",
      "Name of the additional density property that this Material computes");
  params.addRequiredParam<Real>("desired_time_step", "The desired time step.");
  params.addRangeCheckedParam<Real>(
      "safety_factor",
      0.7,
      "(safety_factor>0) & (safety_factor<=1)",
      "The scaled density that this Material produces will potentially allow stable time-step "
      "sizes of desired_time_step / safety_factor.  In practice, however, using such a time step "
      "might result in instabilities, because of time-step lagging and the approximate critical "
      "time-step formula used by this Material.  Hence, safety_factor allows for a safety margin.");
  return params;
}

template <bool is_ad>
DensityScalingTempl<is_ad>::DensityScalingTempl(const InputParameters & parameters)
  : Material(parameters),
    _desired_time_step(this->template getParam<Real>("desired_time_step")),
    _density_scaled(this->template declareGenericProperty<Real, is_ad>(
        this->template getParam<MaterialPropertyName>("scaled_density"))),
    _additional_density(this->template declareGenericProperty<Real, is_ad>(
        this->template getParam<MaterialPropertyName>("additional_density"))),
    _material_density(this->template getGenericMaterialProperty<Real, is_ad>("true_density")),
    _sqrt_effective_stiffness(
        this->template getGenericMaterialPropertyByName<Real, is_ad>("effective_stiffness")),
    _safety_factor(this->template getParam<Real>("safety_factor"))
{
}

template <bool is_ad>
void
DensityScalingTempl<is_ad>::computeQpProperties()
{
  const GenericReal<is_ad> stable_density = Utility::pow<2>(
      _sqrt_effective_stiffness[_qp] * _desired_time_step / _safety_factor / _current_elem->hmin());

  _density_scaled[_qp] = std::max(stable_density, _material_density[_qp]);
  _additional_density[_qp] = _density_scaled[_qp] - _material_density[_qp];
}

template class DensityScalingTempl<false>;
template class DensityScalingTempl<true>;
