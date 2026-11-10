//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MandelConverter.h"

registerMooseObject("SolidMechanicsApp", RankTwoTensorToSymmetricRankTwoTensor);
registerMooseObject("SolidMechanicsApp", ADRankTwoTensorToSymmetricRankTwoTensor);
registerMooseObject("SolidMechanicsApp", SymmetricRankTwoTensorToRankTwoTensor);
registerMooseObject("SolidMechanicsApp", ADSymmetricRankTwoTensorToRankTwoTensor);
registerMooseObject("SolidMechanicsApp", RankFourTensorToSymmetricRankFourTensor);
registerMooseObject("SolidMechanicsApp", ADRankFourTensorToSymmetricRankFourTensor);
registerMooseObject("SolidMechanicsApp", SymmetricRankFourTensorToRankFourTensor);
registerMooseObject("SolidMechanicsApp", ADSymmetricRankFourTensorToRankFourTensor);

template <typename T, bool symmetrize, bool is_ad>
InputParameters
MandelConverter<T, symmetrize, is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription("Converts material property of type " +
                             demangle(typeid(FromType).name()) + " to type " +
                             demangle(typeid(ToType).name()));
  params.addRequiredParam<MaterialPropertyName>("from", "Material property to convert from");
  params.addRequiredParam<MaterialPropertyName>("to", "Material property to convert to");
  return params;
}

template <typename T, bool symmetrize, bool is_ad>
MandelConverter<T, symmetrize, is_ad>::MandelConverter(const InputParameters & params)
  : Material(params),
    _from(this->template getGenericMaterialProperty<FromType, is_ad>("from")),
    _to(this->template declareGenericProperty<ToType, is_ad>("to"))
{
}

template <typename T, bool symmetrize, bool is_ad>
void
MandelConverter<T, symmetrize, is_ad>::initQpStatefulProperties()
{
  computeQpProperties();
}

template <typename T, bool symmetrize, bool is_ad>
void
MandelConverter<T, symmetrize, is_ad>::computeQpProperties()
{
  using GenericToType = Moose::GenericType<ToType, is_ad>;
  _to[_qp] = GenericToType(_from[_qp]);
}

template class MandelConverter<RankTwoTensor, true, false>;
template class MandelConverter<RankTwoTensor, true, true>;
template class MandelConverter<RankTwoTensor, false, false>;
template class MandelConverter<RankTwoTensor, false, true>;
template class MandelConverter<RankFourTensor, true, false>;
template class MandelConverter<RankFourTensor, true, true>;
template class MandelConverter<RankFourTensor, false, false>;
template class MandelConverter<RankFourTensor, false, true>;
