//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MaterialVectorBodyForce.h"
#include "Function.h"

registerMooseObject("SolidMechanicsApp", MaterialVectorBodyForce);
registerMooseObject("SolidMechanicsApp", ADMaterialVectorBodyForce);

template <bool is_ad>
InputParameters
MaterialVectorBodyForceTempl<is_ad>::validParams()
{
  InputParameters params = GenericKernel<is_ad>::validParams();
  params.addClassDescription("Apply a body force vector to the coupled displacement component.");
  params.addParam<FunctionName>(
      "function", "1", "Function to scale the coupled body force vector property");
  params.addParam<Real>(
      "hht_alpha", 0.0, "alpha parameter required for HHT time integration scheme");
  params.addRequiredParam<MaterialPropertyName>("body_force", "Force per unit volume vector");
  params.addCoupledVar("displacements", "The displacements");
  return params;
}

template <bool is_ad>
MaterialVectorBodyForceTempl<is_ad>::MaterialVectorBodyForceTempl(const InputParameters & parameters)
  : GenericKernel<is_ad>(parameters),
    _component(libMesh::invalid_uint),
    _body_force(this->template getGenericMaterialProperty<RealVectorValue, is_ad>("body_force")),
    _function(this->getFunction("function")),
    _alpha(this->template getParam<Real>("hht_alpha"))
{
  for (unsigned int i = 0; i < coupledComponents("displacements"); ++i)
    if (_var.number() == getVar("displacements", i)->number())
      _component = i;

  if (_component == libMesh::invalid_uint)
    this->paramError("variable",
                     "The kernel variable needs to be one of the 'displacements' variables");
}

template <bool is_ad>
GenericReal<is_ad>
MaterialVectorBodyForceTempl<is_ad>::computeQpResidual()
{
  Real factor = _function.value(_t + _alpha * _dt, _q_point[_qp]);
  return -_body_force[_qp](_component) * _test[_i][_qp] * factor;
}

template class MaterialVectorBodyForceTempl<false>;
template class MaterialVectorBodyForceTempl<true>;
