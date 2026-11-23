//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GeneralizedKelvinVoigtBase.h"
#include <optional>

/**
 * This class is an implementation of a generalized Kelvin-Voigt model
 * with constant mechanical properties. It consists of an arbitrary number
 * of Kelvin-Voigt units associated in series with a single spring, and an
 * optional dashpot (this could represent a Burgers model, for example).
 */
template <bool is_ad>
class GeneralizedKelvinVoigtModelTempl : public GeneralizedKelvinVoigtBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  GeneralizedKelvinVoigtModelTempl(const InputParameters & parameters);

protected:
  virtual void computeQpViscoelasticProperties() override;
  virtual void computeQpViscoelasticPropertiesInv() override;

  /**
   * The elasticity tensor associated with the first spring. This is
   * the true elasticity tensor of the material
   */
  RankFourTensor _C0;
  /// The elasticity tensor of each subsequent spring
  std::vector<RankFourTensor> _Ci;
  /// The viscosity of each dashpot
  std::vector<Real> _eta_i;

  /// The inverse of the elasticity tensor of the first spring
  RankFourTensor _S0;
  /// The inverse of each subsequent spring elasticity tensor
  std::vector<RankFourTensor> _Si;

  /// The elasticity tensor associated with the long-term dashpot, if applicable
  std::optional<RankFourTensor> _C_longterm;
  /// The inverse of the elasticity tensor associated with the long-term dashpot, if applicable
  std::optional<RankFourTensor> _S_longterm;

  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_base_name;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_has_longterm_dashpot;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_components;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_first_elasticity_tensor;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_first_elasticity_tensor_inv;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_springs_elasticity_tensors;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_springs_elasticity_tensors_inv;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_dashpot_viscosities;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_longterm_elasticity_tensor;
  using GeneralizedKelvinVoigtBaseTempl<is_ad>::_longterm_elasticity_tensor_inv;
  using Material::_qp;
};

using GeneralizedKelvinVoigtModel = GeneralizedKelvinVoigtModelTempl<false>;
using ADGeneralizedKelvinVoigtModel = GeneralizedKelvinVoigtModelTempl<true>;
