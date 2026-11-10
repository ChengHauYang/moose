//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeVariableBaseEigenStrain.h"

registerMooseObject("SolidMechanicsApp", ComputeVariableBaseEigenStrain);
registerMooseObject("SolidMechanicsApp", ADComputeVariableBaseEigenStrain);

template <bool is_ad>
InputParameters
ComputeVariableBaseEigenStrainTempl<is_ad>::validParams()
{
  InputParameters params = ComputeEigenstrainBaseTempl<is_ad>::validParams();
  params.addClassDescription("Computes Eigenstrain based on material property tensor base");
  params.addRequiredParam<MaterialPropertyName>("base_tensor_property_name",
                                                "Name of base tensor property");
  params.addParam<MaterialPropertyName>(
      "prefactor", 1.0, "Name of material defining the variable dependence");
  params.addParam<std::vector<Real>>(
      "offset_tensor", "Vector of values defining the constant base tensor for the Eigenstrain");
  return params;
}

template <bool is_ad>
ComputeVariableBaseEigenStrainTempl<is_ad>::ComputeVariableBaseEigenStrainTempl(
    const InputParameters & parameters)
  : ComputeEigenstrainBaseTempl<is_ad>(parameters),
    _base_tensor(this->template getGenericMaterialProperty<RealTensorValue, is_ad>(
        "base_tensor_property_name")),
    _prefactor(this->template getGenericMaterialProperty<Real, is_ad>("prefactor"))
{
  if (this->isParamValid("offset_tensor"))
    _offset_tensor.fillFromInputVector(this->template getParam<std::vector<Real>>("offset_tensor"));
  else
    _offset_tensor.zero();
}

template <bool is_ad>
void
ComputeVariableBaseEigenStrainTempl<is_ad>::computeQpEigenstrain()
{
  GenericRankTwoTensor<is_ad> base_rank_two_tensor = _base_tensor[_qp];
  _eigenstrain[_qp] = base_rank_two_tensor * _prefactor[_qp] + _offset_tensor;
}

template class ComputeVariableBaseEigenStrainTempl<false>;
template class ComputeVariableBaseEigenStrainTempl<true>;
