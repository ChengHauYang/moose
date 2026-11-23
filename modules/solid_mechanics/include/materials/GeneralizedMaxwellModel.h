//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GeneralizedMaxwellBase.h"

/**
 * This class is an implementation of a generalized Maxwell model
 * with constant mechanical properties. It consists of an arbitrary number
 * of Kelvin-Voigt units associated in series with a single spring, and an
 * optional dashpot. Having an optional dashpot may lead to divergence of
 * the time-stepping scheme and should be used with care.
 */
template <bool is_ad>
class GeneralizedMaxwellModelTempl : public GeneralizedMaxwellBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  GeneralizedMaxwellModelTempl(const InputParameters & parameters);

protected:
  virtual void computeQpViscoelasticProperties() override;
  virtual void computeQpViscoelasticPropertiesInv() override;

  /**
   * The elasticity tensor associated with the first spring. This is
   * !not! the true elasticity tensor of the material
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

  using GeneralizedMaxwellBaseTempl<is_ad>::_base_name;
  using GeneralizedMaxwellBaseTempl<is_ad>::_has_longterm_dashpot;
  using GeneralizedMaxwellBaseTempl<is_ad>::_components;
  using GeneralizedMaxwellBaseTempl<is_ad>::_first_elasticity_tensor;
  using GeneralizedMaxwellBaseTempl<is_ad>::_first_elasticity_tensor_inv;
  using GeneralizedMaxwellBaseTempl<is_ad>::_springs_elasticity_tensors;
  using GeneralizedMaxwellBaseTempl<is_ad>::_springs_elasticity_tensors_inv;
  using GeneralizedMaxwellBaseTempl<is_ad>::_dashpot_viscosities;
  using Material::_qp;
};

using GeneralizedMaxwellModel = GeneralizedMaxwellModelTempl<false>;
using ADGeneralizedMaxwellModel = GeneralizedMaxwellModelTempl<true>;
