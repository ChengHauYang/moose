//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeLinearViscoelasticStress.h"

registerMooseObject("SolidMechanicsApp", ComputeLinearViscoelasticStress);
registerMooseObject("SolidMechanicsApp", ADComputeLinearViscoelasticStress);

InputParameters
ComputeLinearViscoelasticStress::validParams()
{
  InputParameters params = ComputeLinearElasticStress::validParams();
  params.addClassDescription("Divides total strain into elastic + creep + eigenstrains");
  params.addParam<std::string>(
      "apparent_creep_strain",
      "apparent_creep_strain",
      "name of the apparent creep strain (defined by a LinearViscoelasticityBase material)");
  params.addParam<std::string>(
      "apparent_elasticity_tensor",
      "apparent_elasticity_tensor",
      "name of the apparent elasticity tensor (defined by a LinearViscoelasticityBase material)");
  params.addParam<std::string>(
      "elasticity_tensor_inv",
      "elasticity_tensor_inv",
      "name of the real compliance tensor (defined by a LinearViscoelasticityBase material)");
  return params;
}

ComputeLinearViscoelasticStress::ComputeLinearViscoelasticStress(const InputParameters & parameters)
  : ComputeLinearElasticStress(parameters),
    _creep_strain(declareProperty<RankTwoTensor>(
        isParamValid("base_name") ? _base_name + "_creep_strain" : "creep_strain")),
    _creep_strain_old(getMaterialPropertyOld<RankTwoTensor>(
        isParamValid("base_name") ? _base_name + "_creep_strain" : "creep_strain")),
    _apparent_creep_strain(getMaterialProperty<RankTwoTensor>("apparent_creep_strain")),
    _apparent_elasticity_tensor(getMaterialProperty<RankFourTensor>("apparent_elasticity_tensor")),
    _elasticity_tensor_inv(getMaterialProperty<RankFourTensor>("elasticity_tensor_inv"))
{
}

void
ComputeLinearViscoelasticStress::initQpStatefulProperties()
{
  _creep_strain[_qp].zero();
}

void
ComputeLinearViscoelasticStress::computeQpStress()
{
  _creep_strain[_qp] =
      _mechanical_strain[_qp] - (_apparent_elasticity_tensor[_qp] * _elasticity_tensor_inv[_qp]) *
                                    (_mechanical_strain[_qp] - _apparent_creep_strain[_qp]);

  _elastic_strain[_qp] = _mechanical_strain[_qp] - _creep_strain[_qp];

  _stress[_qp] = _elasticity_tensor[_qp] * _elastic_strain[_qp];

  _Jacobian_mult[_qp] = _elasticity_tensor[_qp];
}

// ---------------------------------------------------------------------------
// AD version
// ---------------------------------------------------------------------------

InputParameters
ADComputeLinearViscoelasticStress::validParams()
{
  InputParameters params = ADComputeStressBase::validParams();
  params.addClassDescription(
      "AD version of ComputeLinearViscoelasticStress: divides total strain into elastic + creep "
      "using apparent properties from ADLinearViscoelasticityManager, providing exact derivatives "
      "of the stress residual with respect to the displacement degrees of freedom.");
  params.addParam<std::string>(
      "apparent_creep_strain",
      "apparent_creep_strain",
      "name of the apparent creep strain (defined by an ADGeneralizedKelvin/MaxwellModel)");
  params.addParam<std::string>(
      "apparent_elasticity_tensor",
      "apparent_elasticity_tensor",
      "name of the apparent elasticity tensor (defined by an ADGeneralizedKelvin/MaxwellModel)");
  params.addParam<std::string>(
      "elasticity_tensor_inv",
      "elasticity_tensor_inv",
      "name of the real compliance tensor (defined by an ADGeneralizedKelvin/MaxwellModel)");
  return params;
}

ADComputeLinearViscoelasticStress::ADComputeLinearViscoelasticStress(
    const InputParameters & parameters)
  : ADComputeStressBase(parameters),
    // ADComputeStressBase sets _base_name to "base_name_" (trailing underscore
    // already included), so concatenating "creep_strain" directly produces the
    // correct property name.  The non-AD class uses _base_name + "_creep_strain"
    // because ComputeStressBase stores base_name without the trailing underscore.
    _creep_strain(declareADProperty<RankTwoTensor>(
        isParamValid("base_name") ? _base_name + "creep_strain" : "creep_strain")),
    // Retrieved only to register creep_strain as a stateful property so MOOSE
    // copies it to the old-state buffer at each timestep; not read in computeQpStress.
    _creep_strain_old(getMaterialPropertyOld<RankTwoTensor>(
        isParamValid("base_name") ? _base_name + "creep_strain" : "creep_strain")),
    _apparent_creep_strain(
        getMaterialProperty<RankTwoTensor>(getParam<std::string>("apparent_creep_strain"))),
    _apparent_elasticity_tensor(
        getMaterialProperty<RankFourTensor>(getParam<std::string>("apparent_elasticity_tensor"))),
    _elasticity_tensor(
        getADMaterialProperty<RankFourTensor>(_base_name + "elasticity_tensor")),
    _elasticity_tensor_inv(
        getMaterialProperty<RankFourTensor>(getParam<std::string>("elasticity_tensor_inv")))
{
}

void
ADComputeLinearViscoelasticStress::initQpStatefulProperties()
{
  ADComputeStressBase::initQpStatefulProperties();
  _creep_strain[_qp] = RankTwoTensor();
}

void
ADComputeLinearViscoelasticStress::computeQpStress()
{
  // creep = mechanical - (C_apparent * C_inv) * (mechanical - apparent_creep)
  _creep_strain[_qp] =
      _mechanical_strain[_qp] -
      (_apparent_elasticity_tensor[_qp] * _elasticity_tensor_inv[_qp]) *
          (_mechanical_strain[_qp] - _apparent_creep_strain[_qp]);

  _elastic_strain[_qp] = _mechanical_strain[_qp] - _creep_strain[_qp];

  _stress[_qp] = _elasticity_tensor[_qp] * _elastic_strain[_qp];
}
