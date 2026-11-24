//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MomentBalancing.h"

// MOOSE includes
#include "ElasticityTensorTools.h"
#include "Material.h"
#include "MooseVariable.h"
#include "PermutationTensor.h"
#include "RankFourTensor.h"
#include "RankTwoTensor.h"

registerMooseObject("SolidMechanicsApp", MomentBalancing);
registerMooseObject("SolidMechanicsApp", ADMomentBalancing);

template <bool is_ad>
InputParameters
MomentBalancingTempl<is_ad>::validParams()
{
  InputParameters params = GenericKernel<is_ad>::validParams();
  params.addClassDescription("Balance of momentum for three-dimensional Cosserat media, notably in "
                             "a Cosserat layered elasticity model.");
  params.addRequiredRangeCheckedParam<unsigned int>(
      "component",
      "component<3",
      "An integer corresponding to the direction the variable this "
      "kernel acts in. (0 for x, 1 for y, 2 for z)");
  params.addParam<std::string>(
      "appended_property_name", "", "Name appended to material properties to make them unique");
  params.addRequiredCoupledVar("Cosserat_rotations", "The 3 Cosserat rotation variables");
  params.addRequiredCoupledVar("displacements", "The 3 displacement variables");
  params.set<bool>("use_displaced_mesh") = false;

  return params;
}

template <bool is_ad>
MomentBalancingTempl<is_ad>::MomentBalancingTempl(const InputParameters & parameters)
  : GenericKernel<is_ad>(parameters),
    _stress(this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(
        "stress" + this->template getParam<std::string>("appended_property_name"))),
    _Jacobian_mult(
        is_ad ? nullptr
              : &this->template getMaterialProperty<RankFourTensor>(
                    "Jacobian_mult" +
                    this->template getParam<std::string>("appended_property_name"))),
    _component(this->template getParam<unsigned int>("component")),
    _nrots(this->coupledComponents("Cosserat_rotations")),
    _wc_var(_nrots),
    _ndisp(this->coupledComponents("displacements")),
    _disp_var(_ndisp)
{
  if (_nrots != 3)
    mooseError("MomentBalancing: This Kernel is only defined for 3-dimensional simulations so 3 "
               "Cosserat rotation variables are needed");
  for (unsigned i = 0; i < _nrots; ++i)
    _wc_var[i] = this->coupled("Cosserat_rotations", i);

  if (_ndisp != 3)
    mooseError("MomentBalancing: This Kernel is only defined for 3-dimensional simulations so 3 "
               "displacement variables are needed");
  for (unsigned i = 0; i < _ndisp; ++i)
    _disp_var[i] = this->coupled("displacements", i);

  // Following check is necessary to ensure the correct Jacobian is calculated
  if (_wc_var[_component] != this->_var.number())
    mooseError("MomentBalancing: The variable for this Kernel must be equal to the Cosserat "
               "rotation variable defined by the \"component\" and the \"Cosserat_rotations\" "
               "parameters");
}

template <bool is_ad>
GenericReal<is_ad>
MomentBalancingTempl<is_ad>::computeQpResidual()
{
  GenericReal<is_ad> the_sum = 0.0;
  for (const auto j : make_range(Moose::dim))
    for (const auto k : make_range(Moose::dim))
      the_sum += PermutationTensor::eps(_component, j, k) * _stress[this->_qp](j, k);
  return this->_test[this->_i][this->_qp] * the_sum;
}

template <bool is_ad>
Real
MomentBalancingTempl<is_ad>::computeQpJacobian()
{
  if constexpr (is_ad)
    return 0.0;
  else
    return ElasticityTensorTools::momentJacobianWC(
        (*_Jacobian_mult)[this->_qp], _component, _component, this->_test[this->_i][this->_qp],
        this->_phi[this->_j][this->_qp]);
}

template <bool is_ad>
Real
MomentBalancingTempl<is_ad>::computeQpOffDiagJacobian(unsigned int jvar)
{
  if constexpr (is_ad)
    return 0.0;
  else
  {
    // What does 2D look like here?
    for (unsigned v = 0; v < _ndisp; ++v)
      if (jvar == _disp_var[v])
        return ElasticityTensorTools::momentJacobian(
            (*_Jacobian_mult)[this->_qp], _component, v, this->_test[this->_i][this->_qp],
            this->_grad_phi[this->_j][this->_qp]);

    // What does 2D look like here?
    for (unsigned v = 0; v < _nrots; ++v)
      if (jvar == _wc_var[v])
        return ElasticityTensorTools::momentJacobianWC(
            (*_Jacobian_mult)[this->_qp], _component, v, this->_test[this->_i][this->_qp],
            this->_phi[this->_j][this->_qp]);

    return 0.0;
  }
}

template class MomentBalancingTempl<false>;
template class MomentBalancingTempl<true>;
