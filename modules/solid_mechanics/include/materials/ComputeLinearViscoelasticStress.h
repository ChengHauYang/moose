//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ComputeLinearElasticStress.h"
#include "LinearViscoelasticityBase.h"
#include "ADComputeStressBase.h"

/**
 * Computes the stress of a linear viscoelastic material, using total
 * small strains. The mechanical strain is decomposed into the elastic
 * strain + the creep strain, the creep strain itself resulting from
 * a spring-dashpot model.
 *
 * If you need to accomodate other sources of inelastic strains, use
 * a ComputeMultipleInelasticStress material instead, associated with a
 * LinearViscoelasticStressUpdate to represent the creep strain.
 */
class ComputeLinearViscoelasticStress : public ComputeLinearElasticStress
{
public:
  static InputParameters validParams();

  ComputeLinearViscoelasticStress(const InputParameters & parameters);

protected:
  virtual void initQpStatefulProperties() override;
  virtual void computeQpStress() override;

  ///@{ Creep strain variable
  MaterialProperty<RankTwoTensor> & _creep_strain;
  const MaterialProperty<RankTwoTensor> & _creep_strain_old;
  ///@}

  /// Apparent creep strain (extracted from a LinearViscoelasticityBase object)
  const MaterialProperty<RankTwoTensor> & _apparent_creep_strain;
  /// Apparent elasticity tensor (extracted from a LinearViscoelasticityBase object)
  const MaterialProperty<RankFourTensor> & _apparent_elasticity_tensor;
  /// Instantaneous compliance tensor (extracted from a LinearViscoelasticityBase object)
  const MaterialProperty<RankFourTensor> & _elasticity_tensor_inv;
};

/**
 * AD version: computes the stress of a linear viscoelastic material with full derivative
 * tracking through the mechanical strain, using the total small-strain formulation.
 *
 * Uses apparent properties computed by ADLinearViscoelasticityManager at TIMESTEP_BEGIN.
 * The apparent properties are non-AD (history-only), so derivatives flow only through
 * the current mechanical strain.
 *
 * creep   = mechanical - (C_apparent * C_inv) * (mechanical - apparent_creep)
 * elastic = mechanical - creep
 * stress  = C * elastic
 */
class ADComputeLinearViscoelasticStress : public ADComputeStressBase
{
public:
  static InputParameters validParams();

  ADComputeLinearViscoelasticStress(const InputParameters & parameters);

protected:
  virtual void initQpStatefulProperties() override;
  virtual void computeQpStress() override;

  ///@{ Creep strain — stateful output consumed by the manager update at TIMESTEP_BEGIN
  ADMaterialProperty<RankTwoTensor> & _creep_strain;
  const MaterialProperty<RankTwoTensor> & _creep_strain_old;
  ///@}

  /// Apparent creep strain (non-AD, set by ADLinearViscoelasticityManager)
  const MaterialProperty<RankTwoTensor> & _apparent_creep_strain;
  /// Apparent elasticity tensor (non-AD, set by ADLinearViscoelasticityManager)
  const MaterialProperty<RankFourTensor> & _apparent_elasticity_tensor;
  /// Instantaneous elasticity tensor (AD, from ADGeneralizedKelvin/MaxwellModel)
  const ADMaterialProperty<RankFourTensor> & _elasticity_tensor;
  /// Instantaneous compliance tensor (non-AD, set by ADLinearViscoelasticityManager)
  const MaterialProperty<RankFourTensor> & _elasticity_tensor_inv;
};
