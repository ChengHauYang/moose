//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "InertialTorque.h"

#include "MooseVariable.h"
#include "PermutationTensor.h"

registerMooseObject("SolidMechanicsApp", InertialTorque);
registerMooseObject("SolidMechanicsApp", ADInertialTorque);

template <bool is_ad>
InputParameters
InertialTorqueTempl<is_ad>::validParams()
{
  InputParameters params = InertialTorqueParent<is_ad>::validParams();
  params.addClassDescription("Kernel for inertial torque: density * displacement x acceleration");
  params.set<bool>("use_displaced_mesh") = false;
  params.addRequiredRangeCheckedParam<unsigned int>(
      "component",
      "component<3",
      "The component of the Cosserat rotation Variable prescribed to "
      "this Kernel (0 for x, 1 for y, 2 for z)");
  params.addRequiredCoupledVar("displacements", "The 3 displacement variables");
  params.addRequiredCoupledVar("velocities", "The 3 velocity variables");
  params.addRequiredCoupledVar("accelerations", "The 3 acceleration variables");
  params.addRequiredParam<Real>("beta", "beta parameter for Newmark Time integration");
  params.addRequiredParam<Real>("gamma", "gamma parameter for Newmark Time integration");
  params.addParam<MaterialPropertyName>(
      "density", "density", "Name of Material Property that provides the density");
  params.addParam<MaterialPropertyName>("eta",
                                        0.0,
                                        "Name of material property or a constant real "
                                        "number defining the eta parameter for the "
                                        "Rayleigh damping.");
  params.addParam<Real>("alpha",
                        0,
                        "alpha parameter for mass dependent numerical damping induced "
                        "by HHT time integration scheme");
  return params;
}

template <bool is_ad>
InertialTorqueTempl<is_ad>::InertialTorqueTempl(const InputParameters & parameters)
  : InertialTorqueParent<is_ad>(parameters),
    _density(this->template getGenericMaterialProperty<Real, is_ad>("density")),
    _beta(this->template getParam<Real>("beta")),
    _gamma(this->template getParam<Real>("gamma")),
    _eta(this->template getGenericMaterialProperty<Real, is_ad>("eta")),
    _alpha(this->template getParam<Real>("alpha")),
    _component(this->template getParam<unsigned int>("component")),
    _ndisp(this->coupledComponents("displacements")),
    _disp_num(this->coupledIndices("displacements")),
    _disp(this->template coupledGenericValues<is_ad>("displacements")),
    _disp_old(this->coupledValuesOld("displacements")),
    _vel_old(this->coupledValuesOld("velocities")),
    _accel_old(this->coupledValuesOld("accelerations")),
    _accel(_ndisp),
    _vel(_ndisp),
    _daccel(_ndisp)
{
  if (_ndisp != 3 || this->coupledComponents("velocities") != 3 ||
      this->coupledComponents("accelerations") != 3)
    mooseError(
        "InertialTorque: This Kernel is only defined for 3-dimensional simulations so 3 "
        "displacement variables, 3 velocity variables and 3 acceleration variables are needed");
}

template <bool is_ad>
GenericReal<is_ad>
InertialTorqueTempl<is_ad>::computeQpResidual()
{
  for (const auto j : index_range(_disp))
  {
    _accel[j] = 1.0 / _beta *
                ((((*_disp[j])[this->_qp] - (*_disp_old[j])[this->_qp]) / (this->_dt * this->_dt)) -
                 (*_vel_old[j])[this->_qp] / this->_dt - (*_accel_old[j])[this->_qp] * (0.5 - _beta));
    _vel[j] = (*_vel_old[j])[this->_qp] + (this->_dt * (1 - _gamma)) * (*_accel_old[j])[this->_qp] +
              _gamma * this->_dt * _accel[j];
    _accel[j] += _eta[this->_qp] *
                 ((1 + _alpha) * _vel[j] - _alpha * (*_vel_old[j])[this->_qp]);
  }

  GenericReal<is_ad> torque = 0.0;
  for (const auto j : index_range(_disp))
    for (const auto k : index_range(_disp))
      torque += PermutationTensor::eps(_component, j, k) * (*_disp[j])[this->_qp] * _accel[k];
  return this->_test[this->_i][this->_qp] * _density[this->_qp] * torque;
}

template <bool is_ad>
Real
InertialTorqueTempl<is_ad>::computeQpJacobian()
{
  if constexpr (is_ad)
    // ADTimeKernel assembles Jacobians by differentiating computeQpResidual().
    return 0.0;
  else
    return computeQpOffDiagJacobian(this->_var.number());
}

template <bool is_ad>
Real
InertialTorqueTempl<is_ad>::computeQpOffDiagJacobian(unsigned int jvar)
{
  if constexpr (is_ad)
    // ADTimeKernel assembles Jacobians by differentiating computeQpResidual().
    return 0.0;
  else
  {
    const Real displacement_acceleration_derivative = 1.0 / (_beta * this->_dt * this->_dt);
    for (const auto j : index_range(_disp))
    {
      _accel[j] = 1.0 / _beta *
                  ((((*_disp[j])[this->_qp] - (*_disp_old[j])[this->_qp]) / (this->_dt * this->_dt)) -
                   (*_vel_old[j])[this->_qp] / this->_dt - (*_accel_old[j])[this->_qp] * (0.5 - _beta));
      _vel[j] = (*_vel_old[j])[this->_qp] + (this->_dt * (1 - _gamma)) * (*_accel_old[j])[this->_qp] +
                _gamma * this->_dt * _accel[j];
      _accel[j] += _eta[this->_qp] *
                   ((1 + _alpha) * _vel[j] - _alpha * (*_vel_old[j])[this->_qp]);
      _daccel[j] = displacement_acceleration_derivative *
                   (1 + _eta[this->_qp] * (1 + _alpha) * _gamma * this->_dt);
    }

    for (const auto v : index_range(_disp_num))
      if (jvar == _disp_num[v])
      {
        Real torque_derivative = 0.0;
        for (const auto k : index_range(_disp))
          torque_derivative += PermutationTensor::eps(_component, v, k) * _accel[k];
        for (const auto j : index_range(_disp))
          torque_derivative += PermutationTensor::eps(_component, j, v) * (*_disp[j])[this->_qp] * _daccel[v];
        return this->_test[this->_i][this->_qp] * _density[this->_qp] * torque_derivative *
               this->_phi[this->_j][this->_qp];
      }

    return 0.0;
  }
}

template class InertialTorqueTempl<false>;
template class InertialTorqueTempl<true>;
