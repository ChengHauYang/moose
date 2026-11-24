//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "PlasticHeatEnergy.h"
#include "MooseMesh.h"
#include "MooseVariable.h"

registerMooseObject("SolidMechanicsApp", PlasticHeatEnergy);
registerMooseObject("SolidMechanicsApp", ADPlasticHeatEnergy);

template <bool is_ad>
InputParameters
PlasticHeatEnergyTempl<is_ad>::validParams()
{
  InputParameters params = GenericKernel<is_ad>::validParams();
  params.addClassDescription("Plastic heat energy density = coeff * stress * plastic_strain_rate");
  params.addRequiredCoupledVar("displacements",
                               "The string of displacements suitable for the problem statement");
  params.addParam<std::string>("base_name", "Material property base name");
  params.addParam<Real>("coeff", 1.0, "Heat energy density = coeff * stress * plastic_strain_rate");
  return params;
}

template <bool is_ad>
PlasticHeatEnergyTempl<is_ad>::PlasticHeatEnergyTempl(const InputParameters & parameters)
  : GenericKernel<is_ad>(parameters),
    _coeff(this->template getParam<Real>("coeff")),
    _base_name(this->isParamValid("base_name") ? this->template getParam<std::string>("base_name") + "_"
                                              : ""),
    _plastic_heat(
        this->template getGenericMaterialProperty<Real, is_ad>(_base_name + "plastic_heat")),
    _dplastic_heat_dstrain(
        is_ad ? nullptr
              : &this->template getMaterialProperty<RankTwoTensor>(_base_name +
                                                                   "dplastic_heat_dstrain")),
    _ndisp(this->coupledComponents("displacements")),
    _disp_var(_ndisp)
{
  for (unsigned int i = 0; i < _ndisp; ++i)
    _disp_var[i] = this->coupled("displacements", i);

  // Checking for consistency between mesh size and length of the provided displacements vector
  if (_ndisp != this->_mesh.dimension())
    mooseError("PlasticHeatEnergy: The number of displacement variables supplied must match the "
               "mesh dimension.");
}

template <bool is_ad>
GenericReal<is_ad>
PlasticHeatEnergyTempl<is_ad>::computeQpResidual()
{
  return -this->_test[this->_i][this->_qp] * _coeff * _plastic_heat[this->_qp];
}

template <bool is_ad>
Real
PlasticHeatEnergyTempl<is_ad>::computeQpJacobian()
{
  if constexpr (is_ad)
    return 0.0;
  return computeQpOffDiagJacobian(this->_var.number());
}

template <bool is_ad>
Real
PlasticHeatEnergyTempl<is_ad>::computeQpOffDiagJacobian(unsigned int jvar)
{
  if constexpr (is_ad)
    return 0.0;
  else
  {
    for (unsigned int i = 0; i < _ndisp; ++i)
      if (jvar == _disp_var[i])
        return -this->_test[this->_i][this->_qp] * _coeff *
               ((*_dplastic_heat_dstrain)[this->_qp] * this->_grad_phi[this->_j][this->_qp])(i);

    return 0.0;
  }
}

template class PlasticHeatEnergyTempl<false>;
template class PlasticHeatEnergyTempl<true>;
