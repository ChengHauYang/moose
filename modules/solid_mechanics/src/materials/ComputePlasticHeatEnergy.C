//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputePlasticHeatEnergy.h"

registerMooseObject("SolidMechanicsApp", ComputePlasticHeatEnergy);
registerMooseObject("SolidMechanicsApp", ADComputePlasticHeatEnergy);

template <bool is_ad>
InputParameters
ComputePlasticHeatEnergyTempl<is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addParam<std::string>("base_name",
                               "Optional parameter that allows the user to define "
                               "multiple mechanics material systems on the same "
                               "block, i.e. for multiple phases");
  params.addClassDescription("Plastic heat energy density = stress * plastic_strain_rate");
  return params;
}

template <bool is_ad>
ComputePlasticHeatEnergyTempl<is_ad>::ComputePlasticHeatEnergyTempl(const InputParameters & parameters)
  : DerivativeMaterialInterface<Material>(parameters),
    _base_name(this->isParamValid("base_name") ? this->template getParam<std::string>("base_name") + "_" : ""),
    _plastic_strain(this->template getGenericMaterialProperty<RankTwoTensor, is_ad>("plastic_strain")),
    _plastic_strain_old(this->template getMaterialPropertyOld<RankTwoTensor>("plastic_strain")),
    _stress(this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(_base_name + "stress")),
    _Jacobian_mult(is_ad ? nullptr
                         : &this->template getMaterialProperty<RankFourTensor>(_base_name +
                                                                              "Jacobian_mult")),
    _elasticity_tensor(
        is_ad ? nullptr
              : &this->template getMaterialProperty<RankFourTensor>(_base_name + "elasticity_tensor")),
    _plastic_heat(this->template declareGenericProperty<Real, is_ad>(_base_name + "plastic_heat")),
    _dplastic_heat_dstrain(
        is_ad ? nullptr
              : &this->template declareProperty<RankTwoTensor>(_base_name +
                                                                "dplastic_heat_dstrain"))
{
}

template <bool is_ad>
void
ComputePlasticHeatEnergyTempl<is_ad>::computeQpProperties()
{
  _plastic_heat[_qp] =
      _stress[_qp].doubleContraction(_plastic_strain[_qp] - _plastic_strain_old[_qp]) / this->_dt;
  if constexpr (!is_ad)
  {
    if (this->_fe_problem.currentlyComputingJacobian())
    {
      if (_plastic_strain[_qp] == _plastic_strain_old[_qp])
        // no plastic deformation, so _elasticity_tensor = _Jacobian_mult
        (*_dplastic_heat_dstrain)[_qp] = RankTwoTensor();
      else
      {
        (*_dplastic_heat_dstrain)[_qp] =
            (_plastic_strain[_qp] - _plastic_strain_old[_qp])
                .initialContraction((*_Jacobian_mult)[_qp]);
        (*_dplastic_heat_dstrain)[_qp] += _stress[_qp];
        (*_dplastic_heat_dstrain)[_qp] -= _stress[_qp].initialContraction(
            (*_elasticity_tensor)[_qp].invSymm() * (*_Jacobian_mult)[_qp]);
        (*_dplastic_heat_dstrain)[_qp] /= this->_dt;
      }
    }
  }
}

template class ComputePlasticHeatEnergyTempl<false>;
template class ComputePlasticHeatEnergyTempl<true>;
