//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MultiPhaseStressMaterial.h"
#include "RankTwoTensor.h"
#include "RankFourTensor.h"

registerMooseObject("SolidMechanicsApp", MultiPhaseStressMaterial);
registerMooseObject("SolidMechanicsApp", ADMultiPhaseStressMaterial);

template <bool is_ad>
InputParameters
MultiPhaseStressMaterialTempl<is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription("Compute a global stress from multiple phase stresses");
  params.addParam<std::vector<MaterialPropertyName>>(
      "h", "Switching Function Materials that provide h(eta_i)");
  params.addRequiredParam<std::vector<std::string>>("phase_base",
                                                    "Base names for the Phase strains");
  params.addParam<std::string>("base_name", "Base name for the computed global stress (optional)");
  return params;
}

template <bool is_ad>
MultiPhaseStressMaterialTempl<is_ad>::MultiPhaseStressMaterialTempl(
    const InputParameters & parameters)
  : Material(parameters),
    _h_list(this->template getParam<std::vector<MaterialPropertyName>>("h")),
    _n_phase(_h_list.size()),
    _h_eta(_n_phase),
    _phase_base(this->template getParam<std::vector<std::string>>("phase_base")),
    _phase_stress(_n_phase),
    _dphase_stress_dstrain(is_ad ? 0 : _n_phase),
    _base_name(this->isParamValid("base_name")
                   ? this->template getParam<std::string>("base_name") + "_"
                   : ""),
    _stress(this->template declareGenericProperty<RankTwoTensor, is_ad>(_base_name + "stress")),
    _dstress_dstrain(
        is_ad ? nullptr
              : &this->template declareProperty<RankFourTensor>(_base_name + "Jacobian_mult"))
{
  // verify parameter length
  if (_n_phase != _phase_base.size())
    mooseError(
        "h and phase_base input vectors need to have the same length in MultiPhaseStressMaterial ",
        name());

  for (const auto i : index_range(_h_eta))
  {
    _h_eta[i] = &this->template getGenericMaterialProperty<Real, is_ad>(_h_list[i]);
    _phase_stress[i] = &this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(
        _phase_base[i] + "_stress");
    if constexpr (!is_ad)
      _dphase_stress_dstrain[i] =
          &this->template getMaterialProperty<RankFourTensor>(_phase_base[i] + "_Jacobian_mult");
  }
}

template <bool is_ad>
void
MultiPhaseStressMaterialTempl<is_ad>::computeQpProperties()
{
  _stress[_qp].zero();
  if constexpr (!is_ad)
    (*_dstress_dstrain)[_qp].zero();

  for (const auto i : index_range(_h_eta))
  {
    _stress[_qp] += (*_h_eta[i])[_qp] * (*_phase_stress[i])[_qp];
    if constexpr (!is_ad)
      (*_dstress_dstrain)[_qp] += (*_h_eta[i])[_qp] * (*_dphase_stress_dstrain[i])[_qp];
  }
}

template class MultiPhaseStressMaterialTempl<false>;
template class MultiPhaseStressMaterialTempl<true>;
