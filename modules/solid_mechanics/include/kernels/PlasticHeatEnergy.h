//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GenericKernel.h"
#include "RankTwoTensor.h"

// Forward Declarations

/**
 * Provides a heat source from plastic deformation:
 * coeff * stress * plastic_strain_rate
 */
template <bool is_ad>
class PlasticHeatEnergyTempl : public GenericKernel<is_ad>
{
public:
  static InputParameters validParams();

  PlasticHeatEnergyTempl(const InputParameters & parameters);

protected:
  virtual GenericReal<is_ad> computeQpResidual() override;

  virtual Real computeQpJacobian() override;

  virtual Real computeQpOffDiagJacobian(unsigned int jvar) override;

  /// coefficient of stress * plastic_strain_rate
  const Real _coeff;

  /// optional parameter that allows multiple mechanics models to be defined
  const std::string _base_name;

  /// stress * plastic_strain_rate
  const GenericMaterialProperty<Real, is_ad> & _plastic_heat;

  /// d(plastic_heat)/d(total_strain), used by the non-AD implementation
  const MaterialProperty<RankTwoTensor> * const _dplastic_heat_dstrain;

  /// umber of coupled displacement variables
  const unsigned int _ndisp;

  /// MOOSE variable number for the displacement variables
  std::vector<unsigned int> _disp_var;

  using GenericKernel<is_ad>::_i;
  using GenericKernel<is_ad>::_qp;
  using GenericKernel<is_ad>::_test;
  using GenericKernel<is_ad>::_phi;
  using GenericKernel<is_ad>::_grad_phi;
  using GenericKernel<is_ad>::_var;
};

using PlasticHeatEnergy = PlasticHeatEnergyTempl<false>;
using ADPlasticHeatEnergy = PlasticHeatEnergyTempl<true>;
