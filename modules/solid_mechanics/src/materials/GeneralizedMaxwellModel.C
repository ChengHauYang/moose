//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GeneralizedMaxwellModel.h"

registerMooseObject("SolidMechanicsApp", GeneralizedMaxwellModel);
registerMooseObject("SolidMechanicsApp", ADGeneralizedMaxwellModel);

template <bool is_ad>
InputParameters
GeneralizedMaxwellModelTempl<is_ad>::validParams()
{
  InputParameters params = GeneralizedMaxwellBaseTempl<is_ad>::validParams();
  params.addClassDescription(
      "Generalized Maxwell model composed of a parallel assembly of unit Maxwell modules");
  params.addRequiredParam<Real>("young_modulus", "initial elastic modulus of the material");
  params.addRequiredParam<Real>("poisson_ratio", "initial poisson ratio of the material");
  params.addRequiredParam<std::vector<Real>>(
      "creep_modulus", "list of the elastic moduli of the different springs in the material");
  params.addRequiredParam<std::vector<Real>>(
      "creep_viscosity",
      "list of the characteristic times of the different dashpots in the material");
  params.addParam<std::vector<Real>>(
      "creep_ratio", "list of the poisson ratios of the different springs in the material");
  params.set<bool>("force_recompute_properties") = false;
  params.suppressParameter<bool>("force_recompute_properties");
  return params;
}

template <bool is_ad>
GeneralizedMaxwellModelTempl<is_ad>::GeneralizedMaxwellModelTempl(
    const InputParameters & parameters)
  : GeneralizedMaxwellBaseTempl<is_ad>(parameters),
    _Ci(this->template getParam<std::vector<Real>>("creep_modulus").size()),
    _eta_i(this->template getParam<std::vector<Real>>("creep_viscosity")),
    _Si(this->template getParam<std::vector<Real>>("creep_modulus").size())
{
  if constexpr (is_ad)
  {
    if (this->isParamValid("driving_eigenstrain"))
      this->paramError("driving_eigenstrain",
                       "driving_eigenstrain is not yet supported for the AD viscoelastic workflow; "
                       "use GeneralizedMaxwellModel (non-AD) for this configuration");
  }

  Real young_modulus = this->template getParam<Real>("young_modulus");
  Real poisson_ratio = this->template getParam<Real>("poisson_ratio");

  _C0.fillFromInputVector({young_modulus, poisson_ratio}, RankFourTensor::symmetric_isotropic_E_nu);

  std::vector<Real> creep_modulus = this->template getParam<std::vector<Real>>("creep_modulus");
  std::vector<Real> creep_ratio;
  if (this->isParamValid("creep_ratio"))
    creep_ratio = this->template getParam<std::vector<Real>>("creep_ratio");
  else
    creep_ratio.resize(_Ci.size(), poisson_ratio);

  if (creep_modulus.size() != _Ci.size())
    this->mooseError("incompatible number of creep moduli and viscosities");
  if (creep_ratio.size() != _Ci.size())
    this->mooseError("incompatible number of creep ratios and viscosities");
  if (_Ci.size() != _eta_i.size())
    this->mooseError("incompatible number of creep ratios and viscosities");

  for (unsigned int i = 0; i < _Ci.size(); ++i)
  {
    _Ci[i].fillFromInputVector({creep_modulus[i], creep_ratio[i]},
                               RankFourTensor::symmetric_isotropic_E_nu);
    _Si[i] = _Ci[i].invSymm();
    _C0 -= _Ci[i];
  }

  if (MooseUtils::absoluteFuzzyEqual(_C0.L2norm(), 0.0))
    _S0.zero();
  else
    _S0 = _C0.invSymm();

  for (unsigned int i = 0; i < _eta_i.size(); ++i)
  {
    if (_eta_i[i] < 0 || MooseUtils::absoluteFuzzyEqual(_eta_i[i], 0.0))
      this->mooseError("material viscosity must be strictly > 0");
  }

  _components = _eta_i.size();
  _has_longterm_dashpot = (_eta_i.size() == _Ci.size() + 1);

  this->issueGuarantee(this->_elasticity_tensor_name, Guarantee::ISOTROPIC);
  this->declareViscoelasticProperties();
}

template <bool is_ad>
void
GeneralizedMaxwellModelTempl<is_ad>::computeQpViscoelasticProperties()
{
  _first_elasticity_tensor[_qp] = _C0;

  for (unsigned int i = 0; i < _Ci.size(); ++i)
    (*_springs_elasticity_tensors[i])[_qp] = _Ci[i];

  for (unsigned int i = 0; i < _eta_i.size(); ++i)
    (*_dashpot_viscosities[i])[_qp] = _eta_i[i];
}

template <bool is_ad>
void
GeneralizedMaxwellModelTempl<is_ad>::computeQpViscoelasticPropertiesInv()
{
  (*_first_elasticity_tensor_inv)[_qp] = _S0;

  for (unsigned int i = 0; i < _Si.size(); ++i)
    (*_springs_elasticity_tensors_inv[i])[_qp] = _Si[i];
}

template class GeneralizedMaxwellModelTempl<false>;
template class GeneralizedMaxwellModelTempl<true>;
