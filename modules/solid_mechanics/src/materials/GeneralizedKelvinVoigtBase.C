//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GeneralizedKelvinVoigtBase.h"
#include "metaphysicl/raw_type.h"

template <bool is_ad>
InputParameters
GeneralizedKelvinVoigtBaseTempl<is_ad>::validParams()
{
  InputParameters params = LinearViscoelasticityBaseTempl<is_ad>::validParams();
  params.set<bool>("need_viscoelastic_properties_inverse") = true;
  params.suppressParameter<bool>("need_viscoelastic_properties_inverse");
  return params;
}

template <bool is_ad>
GeneralizedKelvinVoigtBaseTempl<is_ad>::GeneralizedKelvinVoigtBaseTempl(
    const InputParameters & parameters)
  : LinearViscoelasticityBaseTempl<is_ad>(parameters),
    _first_elasticity_tensor_old(
        getMaterialPropertyOld<RankFourTensor>(_base_name + "spring_elasticity_tensor_0")),
    _longterm_elasticity_tensor_inv_old(nullptr)
{
}

template <bool is_ad>
void
GeneralizedKelvinVoigtBaseTempl<is_ad>::declareViscoelasticProperties()
{
  if (_has_longterm_dashpot && _need_viscoelastic_properties_inverse)
  {
    _longterm_elasticity_tensor_inv_old =
        &getMaterialPropertyOld<RankFourTensor>(_base_name + "longterm_elasticity_tensor_inv");
  }
  LinearViscoelasticityBaseTempl<is_ad>::declareViscoelasticProperties();
}

template <bool is_ad>
void
GeneralizedKelvinVoigtBaseTempl<is_ad>::updateQpViscousStrains()
{
  if (_t_step <= 1)
    return;

  RankTwoTensor effective_stress = _first_elasticity_tensor_old[_qp] * _elastic_strain_old[_qp];

  if (_has_driving_eigenstrain)
    effective_stress += _first_elasticity_tensor_old[_qp] * (*_driving_eigenstrain_old)[_qp];

  for (unsigned int i = 0; i < _springs_elasticity_tensors.size(); ++i)
  {
    Real theta_i = computeTheta(_dt_old, (*_dashpot_viscosities_old[i])[_qp]);
    Real gamma = (*_dashpot_viscosities_old[i])[_qp] / (_dt_old * theta_i);
    (*_viscous_strains[i])[_qp] =
        ((*_springs_elasticity_tensors_inv_old[i])[_qp] * effective_stress) /
        (theta_i * (1. + gamma));
    (*_viscous_strains[i])[_qp] += (*_viscous_strains_old[i])[_qp] *
                                   (gamma / (theta_i * (1. + gamma)) - (1. - theta_i) / theta_i);
  }

  if (_has_longterm_dashpot)
  {
    (*_viscous_strains.back())[_qp] =
        ((*_longterm_elasticity_tensor_inv_old)[_qp] * effective_stress) *
        (_dt_old / (*_dashpot_viscosities_old.back())[_qp]);
    (*_viscous_strains.back())[_qp] += (*_viscous_strains_old.back())[_qp];
  }
}

template <bool is_ad>
void
GeneralizedKelvinVoigtBaseTempl<is_ad>::computeQpApparentElasticityTensors()
{
  _elasticity_tensor[_qp] = _first_elasticity_tensor[_qp];
  _elasticity_tensor_inv[_qp] = (*_first_elasticity_tensor_inv)[_qp];
  _apparent_elasticity_tensor_inv[_qp] = (*_first_elasticity_tensor_inv)[_qp];

  for (unsigned int i = 0; i < _springs_elasticity_tensors.size(); ++i)
  {
    Real theta_i = computeTheta(_dt, (*_dashpot_viscosities[i])[_qp]);
    Real gamma = (*_dashpot_viscosities[i])[_qp] / (_dt * theta_i);
    _apparent_elasticity_tensor_inv[_qp] +=
        (*_springs_elasticity_tensors_inv[i])[_qp] / (1. + gamma);
  }

  if (_has_longterm_dashpot)
  {
    Real theta_i = computeTheta(_dt, (*_dashpot_viscosities.back())[_qp]);
    Real gamma = (*_dashpot_viscosities.back())[_qp] / (_dt * theta_i);
    _apparent_elasticity_tensor_inv[_qp] += (*_longterm_elasticity_tensor_inv)[_qp] / gamma;
  }

  _apparent_elasticity_tensor[_qp] = _apparent_elasticity_tensor_inv[_qp].invSymm();
}

template <bool is_ad>
void
GeneralizedKelvinVoigtBaseTempl<is_ad>::computeQpApparentCreepStrain()
{
  _apparent_creep_strain[_qp].zero();

  for (unsigned int i = 0; i < _springs_elasticity_tensors.size(); ++i)
  {
    Real theta_i = computeTheta(_dt, (*_dashpot_viscosities[i])[_qp]);
    Real gamma = (*_dashpot_viscosities[i])[_qp] / (_dt * theta_i);
    _apparent_creep_strain[_qp] += (*_viscous_strains[i])[_qp] * (gamma / (1. + gamma));
  }

  if (_has_longterm_dashpot)
    _apparent_creep_strain[_qp] += (*_viscous_strains.back())[_qp];

  if (_has_driving_eigenstrain)
  {
    RankFourTensor cumulated_driving_tensor;
    cumulated_driving_tensor.zero();
    for (unsigned int i = 0; i < _springs_elasticity_tensors.size(); ++i)
    {
      double theta_i = computeTheta(_dt, (*_dashpot_viscosities[i])[_qp]);
      double gamma = (*_dashpot_viscosities[i])[_qp] / (_dt * theta_i);
      cumulated_driving_tensor += (*_springs_elasticity_tensors_inv[i])[_qp] / (1. + gamma);
    }

    _apparent_creep_strain[_qp] +=
        MetaPhysicL::raw_value(
            (MetaPhysicL::raw_value(_elasticity_tensor[_qp]) * cumulated_driving_tensor) *
            (*_driving_eigenstrain)[_qp]);

    if (_has_longterm_dashpot)
    {
      double theta_i = computeTheta(_dt, (*_dashpot_viscosities.back())[_qp]);
      double gamma = (*_dashpot_viscosities.back())[_qp] / (_dt * theta_i);
      _apparent_creep_strain[_qp] += (*_driving_eigenstrain)[_qp] / gamma;
    }
  }
}

template class GeneralizedKelvinVoigtBaseTempl<false>;
template class GeneralizedKelvinVoigtBaseTempl<true>;
