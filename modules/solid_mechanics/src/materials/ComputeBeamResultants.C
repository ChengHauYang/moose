//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeBeamResultants.h"

registerMooseObject("SolidMechanicsApp", ComputeBeamResultants);
registerMooseObject("SolidMechanicsApp", ADComputeBeamResultants);

template <bool is_ad>
InputParameters
ComputeBeamResultantsTempl<is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription("Compute forces and moments using elasticity");
  return params;
}

template <bool is_ad>
ComputeBeamResultantsTempl<is_ad>::ComputeBeamResultantsTempl(const InputParameters & parameters)
  : Material(parameters),
    _disp_strain_increment(this->template getGenericMaterialProperty<RealVectorValue, is_ad>(
        "mech_disp_strain_increment")),
    _rot_strain_increment(this->template getGenericMaterialProperty<RealVectorValue, is_ad>(
        "mech_rot_strain_increment")),
    _material_stiffness(
        this->template getMaterialPropertyByName<RealVectorValue>("material_stiffness")),
    _material_flexure(
        this->template getMaterialPropertyByName<RealVectorValue>("material_flexure")),
    _total_rotation(this->template getMaterialPropertyByName<RankTwoTensor>("total_rotation")),
    _force(this->template declareGenericProperty<RealVectorValue, is_ad>("forces")),
    _moment(this->template declareGenericProperty<RealVectorValue, is_ad>("moments")),
    _force_old(this->template getMaterialPropertyOld<RealVectorValue>("forces")),
    _moment_old(this->template getMaterialPropertyOld<RealVectorValue>("moments"))
{
}

template <bool is_ad>
void
ComputeBeamResultantsTempl<is_ad>::initQpStatefulProperties()
{
  _force[_qp].zero();
  _moment[_qp].zero();
}

template <bool is_ad>
void
ComputeBeamResultantsTempl<is_ad>::computeQpProperties()
{
  GenericRealVectorValue<is_ad> force_increment;
  force_increment(0) = _material_stiffness[_qp](0) * _disp_strain_increment[_qp](0);
  force_increment(1) = _material_stiffness[_qp](1) * _disp_strain_increment[_qp](1);
  force_increment(2) = _material_stiffness[_qp](2) * _disp_strain_increment[_qp](2);

  _force[_qp] = _total_rotation[0].transpose() * force_increment + _force_old[_qp];

  GenericRealVectorValue<is_ad> moment_increment;
  moment_increment(0) = _material_flexure[_qp](0) * _rot_strain_increment[_qp](0);
  moment_increment(1) = _material_flexure[_qp](1) * _rot_strain_increment[_qp](1);
  moment_increment(2) = _material_flexure[_qp](2) * _rot_strain_increment[_qp](2);

  _moment[_qp] = _total_rotation[0].transpose() * moment_increment + _moment_old[_qp];
}

template class ComputeBeamResultantsTempl<false>;
template class ComputeBeamResultantsTempl<true>;