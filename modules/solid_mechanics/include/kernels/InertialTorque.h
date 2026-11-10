//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "TimeKernel.h"
#include "ADTimeKernel.h"
#include "Material.h"

// Forward Declarations

/**
 * Computes one component of the inertial torque density, \f$\rho (\mathbf{u} \times
 * \mathbf{a})\f$. The acceleration is reconstructed from displacement using Newmark
 * integration and includes the configured Rayleigh damping and HHT contributions.
 */
template <bool is_ad>
using InertialTorqueParent = typename std::conditional<is_ad, ADTimeKernel, TimeKernel>::type;

template <bool is_ad>
class InertialTorqueTempl : public InertialTorqueParent<is_ad>
{
public:
  static InputParameters validParams();

  InertialTorqueTempl(const InputParameters & parameters);

protected:
  virtual GenericReal<is_ad> computeQpResidual() override;

  virtual Real computeQpJacobian() override;

  virtual Real computeQpOffDiagJacobian(unsigned int jvar) override;

private:
  /// density
  const GenericMaterialProperty<Real, is_ad> & _density;

  /// Newmark beta parameter
  const Real _beta;

  /// Newmark gamma parameter
  const Real _gamma;

  /// Rayleigh-damping eta parameter
  const GenericMaterialProperty<Real, is_ad> & _eta;

  /// HHT alpha parameter
  const Real _alpha;

  /**
   * Component of the cross-product desired.  This
   * kernel will calculate the _component component
   * of density * displacement x acceleration
   */
  const unsigned _component;

  /// Number of displacement variables.  This must be 3
  const unsigned _ndisp;

  /// MOOSE internal variable numbers corresponding to the displacements
  const std::vector<unsigned> _disp_num;

  /// Displacements
  const std::vector<const GenericVariableValue<is_ad> *> _disp;

  /// Old value of displacements
  const std::vector<const VariableValue *> _disp_old;

  /// Old value of velocities
  const std::vector<const VariableValue *> _vel_old;

  /// Old value of accelerations
  const std::vector<const VariableValue *> _accel_old;

  /// Acceleration reconstructed from the current displacement.
  std::vector<GenericReal<is_ad>> _accel;

  /// Velocity reconstructed from the current acceleration.
  std::vector<GenericReal<is_ad>> _vel;

  /// Derivative of the damped acceleration with respect to displacement for the non-AD Jacobian.
  std::vector<Real> _daccel;

};

using InertialTorque = InertialTorqueTempl<false>;
using ADInertialTorque = InertialTorqueTempl<true>;
