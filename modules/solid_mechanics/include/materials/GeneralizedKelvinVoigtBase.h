//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "LinearViscoelasticityBase.h"

/**
 * This class represents an assembly of springs and dashpots following
 * a generalized Kelvin-Voigt model (an arbitrary number of Kelvin-Voigt
 * units assembled in series with a single spring at the top).
 *
 * This class does not attribute the mechanical properties to each spring
 * and dashpot. It must be inherited to do so (see GeneralizedKelvinVoigtModel
 * for an example).
 *
 * This class derives from LinearViscoelasticityBase, and thus contains both
 * the apparent mechanical properties of the material, and the internal variables
 * associated to each of the dashpots in the model. It provides the methods
 * required to perform the time stepping scheme associated with viscoelastic models.
 *
 * See LinearViscoelasticityBase for more information
 */
template <bool is_ad>
class GeneralizedKelvinVoigtBaseTempl : public LinearViscoelasticityBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  GeneralizedKelvinVoigtBaseTempl(const InputParameters & parameters);

protected:
  virtual void declareViscoelasticProperties() override;
  virtual void computeQpApparentElasticityTensors() final;
  virtual void computeQpApparentCreepStrain() final;
  virtual void updateQpViscousStrains() final;

  ///@{ old material properties required for the update of the viscoelastic strain
  const MaterialProperty<RankFourTensor> & _first_elasticity_tensor_old;
  const MaterialProperty<RankFourTensor> * _longterm_elasticity_tensor_inv_old;
  ///@}

  using LinearViscoelasticityBaseTempl<is_ad>::_base_name;
  using LinearViscoelasticityBaseTempl<is_ad>::_has_longterm_dashpot;
  using LinearViscoelasticityBaseTempl<is_ad>::_need_viscoelastic_properties_inverse;
  using LinearViscoelasticityBaseTempl<is_ad>::_first_elasticity_tensor;
  using LinearViscoelasticityBaseTempl<is_ad>::_first_elasticity_tensor_inv;
  using LinearViscoelasticityBaseTempl<is_ad>::_apparent_elasticity_tensor;
  using LinearViscoelasticityBaseTempl<is_ad>::_apparent_elasticity_tensor_inv;
  using LinearViscoelasticityBaseTempl<is_ad>::_elasticity_tensor_inv;
  using LinearViscoelasticityBaseTempl<is_ad>::_springs_elasticity_tensors;
  using LinearViscoelasticityBaseTempl<is_ad>::_springs_elasticity_tensors_inv;
  using LinearViscoelasticityBaseTempl<is_ad>::_springs_elasticity_tensors_inv_old;
  using LinearViscoelasticityBaseTempl<is_ad>::_dashpot_viscosities;
  using LinearViscoelasticityBaseTempl<is_ad>::_dashpot_viscosities_old;
  using LinearViscoelasticityBaseTempl<is_ad>::_longterm_elasticity_tensor;
  using LinearViscoelasticityBaseTempl<is_ad>::_longterm_elasticity_tensor_inv;
  using LinearViscoelasticityBaseTempl<is_ad>::_viscous_strains;
  using LinearViscoelasticityBaseTempl<is_ad>::_viscous_strains_old;
  using LinearViscoelasticityBaseTempl<is_ad>::_apparent_creep_strain;
  using LinearViscoelasticityBaseTempl<is_ad>::_elastic_strain_old;
  using LinearViscoelasticityBaseTempl<is_ad>::_has_driving_eigenstrain;
  using LinearViscoelasticityBaseTempl<is_ad>::_driving_eigenstrain;
  using LinearViscoelasticityBaseTempl<is_ad>::_driving_eigenstrain_old;
  using LinearViscoelasticityBaseTempl<is_ad>::computeTheta;
  using LinearViscoelasticityBaseTempl<is_ad>::_components;
  using Material::getMaterialPropertyOld;
  using Material::_qp;
  using Material::_t_step;
  using Material::_dt;
  using Material::_dt_old;
  using ComputeElasticityTensorBaseTempl<is_ad, RankFourTensor>::_elasticity_tensor;
};

using GeneralizedKelvinVoigtBase = GeneralizedKelvinVoigtBaseTempl<false>;
using ADGeneralizedKelvinVoigtBase = GeneralizedKelvinVoigtBaseTempl<true>;
