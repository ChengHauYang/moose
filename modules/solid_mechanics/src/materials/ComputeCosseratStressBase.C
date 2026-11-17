//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeCosseratStressBase.h"

template <bool is_ad>
InputParameters
ComputeCosseratStressBaseTempl<is_ad>::validParams()
{
  InputParameters params = Parent::validParams();
  params.addClassDescription("Compute stress and couple stress in the Cosserat situation");
  return params;
}

template <bool is_ad>
ComputeCosseratStressBaseTempl<is_ad>::ComputeCosseratStressBaseTempl(
    const InputParameters & parameters)
  : Parent(parameters),
    _curvature(this->template getGenericMaterialPropertyByName<RankTwoTensor, is_ad>("curvature")),
    _elastic_flexural_rigidity_tensor(
        this->template getMaterialPropertyByName<RankFourTensor>(
            "elastic_flexural_rigidity_tensor")),
    _stress_couple(
        this->template declareGenericProperty<RankTwoTensor, is_ad>("couple_stress")),
    _Jacobian_mult_couple(is_ad ? nullptr
                               : &this->template declareProperty<RankFourTensor>(
                                     "couple_Jacobian_mult"))
{
}

template <bool is_ad>
void
ComputeCosseratStressBaseTempl<is_ad>::initQpStatefulProperties()
{
  Parent::initQpStatefulProperties();
  _stress_couple[_qp].zero();
}

template class ComputeCosseratStressBaseTempl<false>;
template class ComputeCosseratStressBaseTempl<true>;
