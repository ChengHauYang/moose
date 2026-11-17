//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeCosseratLinearElasticStress.h"

registerMooseObject("SolidMechanicsApp", ComputeCosseratLinearElasticStress);
registerMooseObject("SolidMechanicsApp", ADComputeCosseratLinearElasticStress);

template <bool is_ad>
InputParameters
ComputeCosseratLinearElasticStressTempl<is_ad>::validParams()
{
  InputParameters params = ComputeCosseratStressBaseTempl<is_ad>::validParams();
  params.addClassDescription(
      "Compute Cosserat stress and couple-stress elasticity for small strains");
  return params;
}

template <bool is_ad>
ComputeCosseratLinearElasticStressTempl<is_ad>::ComputeCosseratLinearElasticStressTempl(
    const InputParameters & parameters)
  : ComputeCosseratStressBaseTempl<is_ad>(parameters),
    _elasticity_tensor_name(_base_name + "elasticity_tensor"),
    _elasticity_tensor(
        this->template getMaterialPropertyByName<RankFourTensor>(_elasticity_tensor_name))
{
}

template <bool is_ad>
void
ComputeCosseratLinearElasticStressTempl<is_ad>::initialSetup()
{
  if (this->template hasBlockMaterialProperty<RankTwoTensor>(_base_name + "strain_increment"))
    mooseError("This linear elastic stress calculation only works for small strains");
}

template <bool is_ad>
void
ComputeCosseratLinearElasticStressTempl<is_ad>::computeQpStress()
{
  _stress[_qp] = _elasticity_tensor[_qp] * _mechanical_strain[_qp];
  _stress_couple[_qp] = _elastic_flexural_rigidity_tensor[_qp] * _curvature[_qp];

  _elastic_strain[_qp] = _mechanical_strain[_qp];

  if constexpr (!is_ad)
  {
    this->_Jacobian_mult[_qp] = _elasticity_tensor[_qp];
    (*_Jacobian_mult_couple)[_qp] = _elastic_flexural_rigidity_tensor[_qp];
  }
}

template class ComputeCosseratLinearElasticStressTempl<false>;
template class ComputeCosseratLinearElasticStressTempl<true>;
