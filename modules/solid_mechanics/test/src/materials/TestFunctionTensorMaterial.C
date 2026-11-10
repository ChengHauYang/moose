//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "TestFunctionTensorMaterial.h"

registerMooseObject("SolidMechanicsTestApp", TestFunctionTensorMaterial);
registerMooseObject("SolidMechanicsTestApp", ADTestFunctionTensorMaterial);

template <bool is_ad>
InputParameters
TestFunctionTensorMaterialTempl<is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription(
      "Test-only material that linearly interpolates or extrapolates a tensor-valued material "
      "property from a coupled scalar variable.");
  params.addParam<MaterialPropertyName>(
      "property_name", "test_function_tensor", "Name of the declared tensor property");
  params.addRequiredParam<std::vector<Real>>(
      "tensor0", "9 components of the tensor value at c = 0 (row-major)");
  params.addRequiredParam<std::vector<Real>>(
      "tensor1", "9 components of the tensor value at c = 1 (row-major)");
  params.addRequiredCoupledVar("c", "Coupled variable that blends tensor0 into tensor1");
  return params;
}

template <bool is_ad>
TestFunctionTensorMaterialTempl<is_ad>::TestFunctionTensorMaterialTempl(
    const InputParameters & parameters)
  : Material(parameters),
    _c(this->template coupledGenericValue<is_ad>("c")),
    _prop(this->template declareGenericProperty<RealTensorValue, is_ad>(
        this->template getParam<MaterialPropertyName>("property_name"))),
    _tensor0(this->template getParam<std::vector<Real>>("tensor0")),
    _tensor1(this->template getParam<std::vector<Real>>("tensor1"))
{
  if (_tensor0.size() != 9 || _tensor1.size() != 9)
    paramError("tensor0", "tensor0 and tensor1 must each have exactly 9 components (row-major)");
}

template <bool is_ad>
void
TestFunctionTensorMaterialTempl<is_ad>::computeQpProperties()
{
  for (const auto i : make_range(3))
    for (const auto j : make_range(3))
      _prop[_qp](i, j) =
          (1.0 - _c[_qp]) * _tensor0[i * 3 + j] + _c[_qp] * _tensor1[i * 3 + j];
}

template class TestFunctionTensorMaterialTempl<false>;
template class TestFunctionTensorMaterialTempl<true>;
