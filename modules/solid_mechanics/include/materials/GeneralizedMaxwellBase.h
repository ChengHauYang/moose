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
 * a generalized Maxwell model (an arbitrary number of Maxwell
 * units assembled in parallel with a single spring).
 *
 * This class does not attribute the mechanical properties to each spring
 * and dashpot. It must be inherited to do so (see GeneralizedMaxwellModel
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
class GeneralizedMaxwellBaseTempl : public LinearViscoelasticityBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  GeneralizedMaxwellBaseTempl(const InputParameters & parameters);

protected:
  virtual void computeQpApparentElasticityTensors() final;
  virtual void computeQpApparentCreepStrain() final;
  virtual void updateQpViscousStrains() final;

  using LinearViscoelasticityBaseTempl<is_ad>::_base_name;
  using LinearViscoelasticityBaseTempl<is_ad>::_has_longterm_dashpot;
  using LinearViscoelasticityBaseTempl<is_ad>::_need_viscoelastic_properties_inverse;
  using LinearViscoelasticityBaseTempl<is_ad>::_first_elasticity_tensor;
  using LinearViscoelasticityBaseTempl<is_ad>::_apparent_elasticity_tensor;
  using LinearViscoelasticityBaseTempl<is_ad>::_apparent_elasticity_tensor_inv;
  using LinearViscoelasticityBaseTempl<is_ad>::_elasticity_tensor_inv;
  using LinearViscoelasticityBaseTempl<is_ad>::_springs_elasticity_tensors;
  using LinearViscoelasticityBaseTempl<is_ad>::_dashpot_viscosities;
  using LinearViscoelasticityBaseTempl<is_ad>::_dashpot_viscosities_old;
  using LinearViscoelasticityBaseTempl<is_ad>::_viscous_strains;
  using LinearViscoelasticityBaseTempl<is_ad>::_viscous_strains_old;
  using LinearViscoelasticityBaseTempl<is_ad>::_apparent_creep_strain;
  using LinearViscoelasticityBaseTempl<is_ad>::_elastic_strain_old;
  using LinearViscoelasticityBaseTempl<is_ad>::_creep_strain_old;
  using LinearViscoelasticityBaseTempl<is_ad>::_has_driving_eigenstrain;
  using LinearViscoelasticityBaseTempl<is_ad>::_driving_eigenstrain;
  using LinearViscoelasticityBaseTempl<is_ad>::_driving_eigenstrain_old;
  using LinearViscoelasticityBaseTempl<is_ad>::computeTheta;
  using Material::_qp;
  using Material::_t_step;
  using Material::_dt;
  using Material::_dt_old;
  using ComputeElasticityTensorBaseTempl<is_ad, RankFourTensor>::_elasticity_tensor;
};

using GeneralizedMaxwellBase = GeneralizedMaxwellBaseTempl<false>;
using ADGeneralizedMaxwellBase = GeneralizedMaxwellBaseTempl<true>;
