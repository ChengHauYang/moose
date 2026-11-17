//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "DerivativeMaterialInterface.h"
#include "Material.h"
#include "RankTwoTensor.h"
#include "RankFourTensor.h"

/**
 * ComputePlasticHeatEnergy computes stress * (plastic_strain - plastic_strain_old)
 * and, if currentlyComputingJacobian, then the derivative of this quantity wrt total strain
 */
template <bool is_ad>
class ComputePlasticHeatEnergyTempl : public DerivativeMaterialInterface<Material>
{
public:
  static InputParameters validParams();

  ComputePlasticHeatEnergyTempl(const InputParameters & parameters);

protected:
  virtual void computeQpProperties() override;

  /// optional parameter that allows multiple mechanics materials to be defined
  const std::string _base_name;

  /// plastic strain
  const GenericMaterialProperty<RankTwoTensor, is_ad> & _plastic_strain;

  /// old value of plastic strain
  const MaterialProperty<RankTwoTensor> & _plastic_strain_old;

  /// stress
  const GenericMaterialProperty<RankTwoTensor, is_ad> & _stress;

  /// d(stress)/d(total strain), used by the non-AD implementation
  const MaterialProperty<RankFourTensor> * const _Jacobian_mult;

  /// Elasticity tensor, used by the non-AD implementation
  const MaterialProperty<RankFourTensor> * const _elasticity_tensor;

  /// Computed property: stress * (plastic_strain - plastic_strain_old) / dt
  GenericMaterialProperty<Real, is_ad> & _plastic_heat;

  /// d(plastic_heat)/d(total strain), used by the non-AD implementation
  MaterialProperty<RankTwoTensor> * const _dplastic_heat_dstrain;
};

using ComputePlasticHeatEnergy = ComputePlasticHeatEnergyTempl<false>;
using ADComputePlasticHeatEnergy = ComputePlasticHeatEnergyTempl<true>;
