//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "PoroMechanicsCoupling.h"

// MOOSE includes
#include "Function.h"
#include "MooseMesh.h"
#include "MooseVariable.h"

registerMooseObject("SolidMechanicsApp", PoroMechanicsCoupling);
registerMooseObject("SolidMechanicsApp", ADPoroMechanicsCoupling);

template <bool is_ad>
InputParameters
PoroMechanicsCouplingTempl<is_ad>::validParams()
{
  InputParameters params = GenericKernel<is_ad>::validParams();
  params.addClassDescription(
      "Adds $-Bi \\cdot p_s \\cdot \\nabla \\Psi_c$, where the subscript $c$ is the component.");
  params.addRequiredCoupledVar("porepressure", "Pore pressure, $p_s$.");
  params.addRequiredParam<unsigned int>("component",
                                        "The gradient direction (0 for x, 1 for y and 2 for z)");
  return params;
}

template <bool is_ad>
PoroMechanicsCouplingTempl<is_ad>::PoroMechanicsCouplingTempl(const InputParameters & parameters)
  : GenericKernel<is_ad>(parameters),
    _coefficient(this->template getGenericMaterialProperty<Real, is_ad>("biot_coefficient")),
    _porepressure(this->template coupledGenericValue<is_ad>("porepressure")),
    _porepressure_var_num(this->coupled("porepressure")),
    _component(this->template getParam<unsigned int>("component"))
{
  if (_component >= this->_mesh.dimension())
    mooseError("PoroMechanicsCoupling: component should not be greater than the mesh dimension\n");
}

template <bool is_ad>
GenericReal<is_ad>
PoroMechanicsCouplingTempl<is_ad>::computeQpResidual()
{
  return -_coefficient[_qp] * _porepressure[_qp] * _grad_test[_i][_qp](_component);
}

template <bool is_ad>
Real
PoroMechanicsCouplingTempl<is_ad>::computeQpJacobian()
{
  if constexpr (is_ad)
    // ADKernel assembles Jacobians by differentiating computeQpResidual().
    return 0.0;
  else
  {
    if (_var.number() != _porepressure_var_num)
      return 0.0;
    return -_coefficient[_qp] * _phi[_j][_qp] * _grad_test[_i][_qp](_component);
  }
}

template <bool is_ad>
Real
PoroMechanicsCouplingTempl<is_ad>::computeQpOffDiagJacobian(unsigned int jvar)
{
  if constexpr (is_ad)
    // ADKernel assembles Jacobians by differentiating computeQpResidual().
    return 0.0;
  else
  {
    if (jvar != _porepressure_var_num)
      return 0.0;
    return -_coefficient[_qp] * _phi[_j][_qp] * _grad_test[_i][_qp](_component);
  }
}

template class PoroMechanicsCouplingTempl<false>;
template class PoroMechanicsCouplingTempl<true>;
