//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SumTensorIncrements.h"
#include "libmesh/quadrature.h"

registerMooseObject("SolidMechanicsApp", SumTensorIncrements);
registerMooseObject("SolidMechanicsApp", ADSumTensorIncrements);

template <bool is_ad>
InputParameters
SumTensorIncrementsTempl<is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription("Compute tensor property by summing tensor increments");
  params.addRequiredParam<MaterialPropertyName>("tensor_name", "Name of strain property");
  params.addParam<std::vector<MaterialPropertyName>>("coupled_tensor_increment_names",
                                                     "Name of strain increment properties");
  return params;
}

template <bool is_ad>
SumTensorIncrementsTempl<is_ad>::SumTensorIncrementsTempl(const InputParameters & parameters)
  : DerivativeMaterialInterface<Material>(parameters),
    _property_names(getParam<std::vector<MaterialPropertyName>>("coupled_tensor_increment_names")),
    _tensor(this->template declareGenericProperty<RankTwoTensor, is_ad>(
        this->template getParam<MaterialPropertyName>("tensor_name"))),
    _tensor_old(
        this->template getMaterialPropertyOld<RankTwoTensor>(
            this->template getParam<MaterialPropertyName>("tensor_name"))),
    _tensor_increment(this->template declareGenericProperty<RankTwoTensor, is_ad>(
        this->template getParam<MaterialPropertyName>("tensor_name") + "_increment"))
{
  _num_property = _property_names.size();

  if (_num_property > 0)
  {
    _coupled_tensor_increments.resize(_num_property);

    for (const auto i : index_range(_coupled_tensor_increments))
      _coupled_tensor_increments[i] =
          &this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(_property_names[i]);
  }
}

template <bool is_ad>
void
SumTensorIncrementsTempl<is_ad>::initQpStatefulProperties()
{
  _tensor[this->_qp].zero();
  _tensor_increment[this->_qp].zero();
}

template <bool is_ad>
void
SumTensorIncrementsTempl<is_ad>::computeQpProperties()
{
  _tensor_increment[this->_qp].zero();

  for (const auto i : index_range(_coupled_tensor_increments))
    _tensor_increment[this->_qp] += (*_coupled_tensor_increments[i])[this->_qp];

  _tensor[this->_qp] = _tensor_old[this->_qp] + _tensor_increment[this->_qp];
}

template class SumTensorIncrementsTempl<false>;
template class SumTensorIncrementsTempl<true>;