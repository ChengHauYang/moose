//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "LinearViscoelasticStressUpdate.h"

registerMooseObject("SolidMechanicsApp", LinearViscoelasticStressUpdate);
registerMooseObject("SolidMechanicsApp", ADLinearViscoelasticStressUpdate);

InputParameters
LinearViscoelasticStressUpdate::validParams()
{
  InputParameters params = StressUpdateBase::validParams();
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

LinearViscoelasticStressUpdate::LinearViscoelasticStressUpdate(const InputParameters & parameters)
  : StressUpdateBase(parameters),
    _creep_strain(declareProperty<RankTwoTensor>(_base_name + "creep_strain")),
    _creep_strain_old(getMaterialPropertyOld<RankTwoTensor>(_base_name + "creep_strain")),
    _apparent_creep_strain(getMaterialProperty<RankTwoTensor>("apparent_creep_strain")),
    _apparent_elasticity_tensor(getMaterialProperty<RankFourTensor>("apparent_elasticity_tensor")),
    _elasticity_tensor_inv(getMaterialProperty<RankFourTensor>("elasticity_tensor_inv"))
{
}

void
LinearViscoelasticStressUpdate::initQpStatefulProperties()
{
  _creep_strain[_qp].zero();
}

void
LinearViscoelasticStressUpdate::propagateQpStatefulProperties()
{
  _creep_strain[_qp] = _creep_strain_old[_qp];
}

void
LinearViscoelasticStressUpdate::updateState(RankTwoTensor & strain_increment,
                                            RankTwoTensor & inelastic_strain_increment,
                                            const RankTwoTensor & /*rotation_increment*/,
                                            RankTwoTensor & stress_new,
                                            const RankTwoTensor & /*stress_old*/,
                                            const RankFourTensor & elasticity_tensor,
                                            const RankTwoTensor & elastic_strain_old,
                                            bool /*compute_full_tangent_operator*/,
                                            RankFourTensor & /*tangent_operator*/)
{
  RankTwoTensor current_mechanical_strain =
      elastic_strain_old + _creep_strain_old[_qp] + strain_increment;

  _creep_strain[_qp] =
      current_mechanical_strain - (_apparent_elasticity_tensor[_qp] * _elasticity_tensor_inv[_qp]) *
                                      (current_mechanical_strain - _apparent_creep_strain[_qp]);

  RankTwoTensor creep_strain_increment = _creep_strain[_qp] - _creep_strain_old[_qp];

  strain_increment -= creep_strain_increment;
  inelastic_strain_increment += creep_strain_increment;
  stress_new -= elasticity_tensor * creep_strain_increment;
}

// ---------------------------------------------------------------------------
// AD version
// ---------------------------------------------------------------------------

InputParameters
ADLinearViscoelasticStressUpdate::validParams()
{
  InputParameters params = ADStressUpdateBase::validParams();
  params.addClassDescription(
      "AD version of LinearViscoelasticStressUpdate for use with ADComputeMultipleInelasticStress. "
      "Uses apparent properties from ADLinearViscoelasticityManager at TIMESTEP_BEGIN and provides "
      "exact displacement derivatives for the incremental small-strain viscoelastic stress.");
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

ADLinearViscoelasticStressUpdate::ADLinearViscoelasticStressUpdate(
    const InputParameters & parameters)
  : ADStressUpdateBase(parameters),
    _creep_strain(declareADProperty<RankTwoTensor>(_base_name + "creep_strain")),
    _creep_strain_old(getMaterialPropertyOld<RankTwoTensor>(_base_name + "creep_strain")),
    _apparent_creep_strain(
        getMaterialProperty<RankTwoTensor>(getParam<std::string>("apparent_creep_strain"))),
    _apparent_elasticity_tensor(
        getMaterialProperty<RankFourTensor>(getParam<std::string>("apparent_elasticity_tensor"))),
    _elasticity_tensor_inv(
        getMaterialProperty<RankFourTensor>(getParam<std::string>("elasticity_tensor_inv")))
{
}

void
ADLinearViscoelasticStressUpdate::initQpStatefulProperties()
{
  _creep_strain[_qp] = ADRankTwoTensor();
}

void
ADLinearViscoelasticStressUpdate::propagateQpStatefulProperties()
{
  _creep_strain[_qp] = _creep_strain_old[_qp];
}

void
ADLinearViscoelasticStressUpdate::updateState(ADRankTwoTensor & strain_increment,
                                              ADRankTwoTensor & inelastic_strain_increment,
                                              const ADRankTwoTensor & /*rotation_increment*/,
                                              ADRankTwoTensor & stress_new,
                                              const RankTwoTensor & /*stress_old*/,
                                              const ADRankFourTensor & elasticity_tensor,
                                              const RankTwoTensor & elastic_strain_old,
                                              bool /*compute_full_tangent_operator*/,
                                              RankFourTensor & /*tangent_operator*/)
{
  ADRankTwoTensor current_mechanical_strain =
      elastic_strain_old + _creep_strain_old[_qp] + strain_increment;

  _creep_strain[_qp] =
      current_mechanical_strain -
      (_apparent_elasticity_tensor[_qp] * _elasticity_tensor_inv[_qp]) *
          (current_mechanical_strain - _apparent_creep_strain[_qp]);

  ADRankTwoTensor creep_strain_increment = _creep_strain[_qp] - _creep_strain_old[_qp];

  strain_increment -= creep_strain_increment;
  inelastic_strain_increment += creep_strain_increment;
  stress_new -= elasticity_tensor * creep_strain_increment;
}
