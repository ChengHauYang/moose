//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "CZMComputeLocalTractionTotalBase.h"

/**
 * Implementation of the mixed mode bilinear traction separation law
 * described in Mixed-Mode Decohesion Finite Elements for the Simulation of Delamination in
 *Composite Materials, Pedro P. Camanho and Carlos G. Davila, NASA/TM-2002-211737
 **/
class BiLinearMixedModeTraction : public CZMComputeLocalTractionTotalBase
{
public:
  /// Stateful and lagged history values consumed by computeTrialTraction()
  struct OldState
  {
    Real damage = 0.0;
    RealVectorValue interface_displacement_jump;
  };

  /// Local traction, derivative, candidate damage, and scratch scalar fields
  struct TrialTractionResult
  {
    RealVectorValue traction;
    RankTwoTensor dtraction_djump;
    Real damage = 0.0;
    Real beta = 0.0;
    Real delta_init = 0.0;
    Real delta_final = 0.0;
    Real delta_m = 0.0;
  };

  static InputParameters validParams();
  BiLinearMixedModeTraction(const InputParameters & parameters);

  /// Evaluate the local traction, derivative, and candidate history for an explicit trial jump
  TrialTractionResult computeTrialTraction(const RealVectorValue & trial_jump,
                                           const OldState & old_state) const;

protected:
  virtual void initQpStatefulProperties() override;

  virtual void computeInterfaceTractionAndDerivatives() override;

  /// The traction-separation law
  virtual RealVectorValue computeTraction();

  /// Compute the total traction derivatives w.r.t. the interface displacement jump
  virtual RankTwoTensor computeTractionDerivatives();

  // The damage evolution law
  virtual void computeDamage();

  /// Penalty elastic stiffness
  const Real _K;

  ///@{
  /// damage variable
  MaterialProperty<Real> & _d;
  const MaterialProperty<Real> & _d_old;
  ///@}

  /// old interface displacement jump value
  const MaterialProperty<RealVectorValue> & _interface_displacement_jump_old;

  // @{
  // The parameters in the damage evolution law
  MaterialProperty<Real> & _delta_init;
  MaterialProperty<Real> & _delta_final;
  MaterialProperty<Real> & _delta_m;
  // @}

  /// Mode I critical fracture toughness
  const MaterialProperty<Real> & _GI_c;

  /// Mode II critical fracture toughness
  const MaterialProperty<Real> & _GII_c;

  /// The normal strength
  const MaterialProperty<Real> & _N;

  /// The shear strength
  const MaterialProperty<Real> & _S;

  /// The B-K power law parameter
  const Real _eta;

  /// The mode mixity ratio
  MaterialProperty<Real> & _beta;

  /// The viscosity
  const Real _viscosity;

  /// mixed mode propagation criterion
  enum class MixedModeCriterion
  {
    POWER_LAW,
    BK
  } _criterion;

private:
  // @{
  // Parameters to improve numerical convergence
  const bool _lag_mode_mixity;
  const bool _lag_disp_jump;
  const Real _alpha;
  // @}
};
