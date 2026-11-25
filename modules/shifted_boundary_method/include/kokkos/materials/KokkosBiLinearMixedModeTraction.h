//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceMaterial.h"
#include "SCZMBiLinearCohesiveLaw.h"
#include "SCZMTrialTractionProvider.h"

/**
 * Computes the mixed-mode bilinear cohesive traction-separation law of BiLinearMixedModeTraction
 * with Kokkos and supplies it as the trial law of weighted traction SCZM stabilization.
 *
 * Under weighted traction stabilization the cohesive law acts at the auxiliary jump of the
 * generalized Robin solve rather than at the interface jump, so the damage and the lagged history
 * jump are recorded by recordTrialState() at the converged auxiliary jump instead of by
 * computeQpProperties().
 */
class KokkosBiLinearMixedModeTraction : public Moose::Kokkos::InterfaceMaterial,
                                        public SCZMTrialTractionProvider
{
public:
  static InputParameters validParams();

  KokkosBiLinearMixedModeTraction(const InputParameters & parameters);

  void initialSetup() override;

  template <typename Derived>
  KOKKOS_FUNCTION void initQpStatefulProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                                Datum & elem_datum,
                                                Datum &,
                                                unsigned int qp,
                                                unsigned int) const
  {
    _damage(elem_datum, qp) = 0.0;
    auto history_jump = _history_jump(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
      history_jump(i) = 0.0;
  }

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                           Datum & elem_datum,
                                           Datum &,
                                           unsigned int qp,
                                           unsigned int) const;

  /// Builds the trial law at a QP from the committed damage and history jump of the last step
  template <typename DatumType>
  KOKKOS_FUNCTION Moose::Kokkos::SCZM::BiLinearMixedModeCohesiveLaw trialLaw(DatumType & elem_datum,
                                                                             unsigned int qp) const;

  /// Records the damage and history jump at the converged local auxiliary jump of a QP
  template <typename DatumType>
  KOKKOS_FUNCTION void recordTrialState(DatumType & elem_datum,
                                        unsigned int qp,
                                        const Moose::Kokkos::Real3 & local_jump) const
  {
    recordState(trialLaw(elem_datum, qp), elem_datum, qp, local_jump);
  }

  void robinPrepass(const KokkosSCZMStabilizationData & uo) const override;
  void checkStabilizationLaw(const KokkosSCZMStabilizationData & uo) const override;

private:
  template <typename DatumType>
  KOKKOS_FUNCTION void recordState(const Moose::Kokkos::SCZM::BiLinearMixedModeCohesiveLaw & law,
                                   DatumType & elem_datum,
                                   unsigned int qp,
                                   const Moose::Kokkos::Real3 & local_jump) const
  {
    _damage(elem_datum, qp) = law.candidateDamage(local_jump, _ndisp);
    auto history_jump = _history_jump(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
      history_jump(i) = local_jump(i);
  }

  const unsigned int _ndisp;
  const Real _penalty_stiffness;
  const Real _eta;
  const Real _viscosity;
  const Moose::Kokkos::SCZM::BiLinearMixedModeCohesiveLaw::MixedModeCriterion _criterion;
  const bool _lag_mode_mixity;
  const bool _lag_displacement_jump;
  const Real _alpha;

  Moose::Kokkos::MaterialProperty<Real> _damage;
  Moose::Kokkos::MaterialProperty<Real> _damage_old;
  /// Local jump the damage was last evaluated at; its old value is the lagged jump of the law
  Moose::Kokkos::MaterialProperty<Real, 1> _history_jump;
  Moose::Kokkos::MaterialProperty<Real, 1> _history_jump_old;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_displacement_jump;
  Moose::Kokkos::MaterialProperty<Real> _GI_c;
  Moose::Kokkos::MaterialProperty<Real> _GII_c;
  Moose::Kokkos::MaterialProperty<Real> _normal_strength;
  Moose::Kokkos::MaterialProperty<Real> _shear_strength;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_traction;
  Moose::Kokkos::MaterialProperty<Real, 2> _dinterface_traction_djump;

  /// Whether weighted traction stabilization records the state through recordTrialState()
  bool _stabilized = false;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosBiLinearMixedModeTraction::computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                                     Datum & elem_datum,
                                                     Datum &,
                                                     const unsigned int qp,
                                                     const unsigned int) const
{
  Moose::Kokkos::Real3 jump{};
  const auto displacement_jump = _interface_displacement_jump(elem_datum, qp);
  for (unsigned int i = 0; i < _ndisp; ++i)
    jump(i) = displacement_jump(i);

  const auto law = trialLaw(elem_datum, qp);
  const auto evaluation = law.evaluate(jump, _ndisp);
  auto traction = _interface_traction(elem_datum, qp);
  auto tangent = _dinterface_traction_djump(elem_datum, qp);
  for (unsigned int i = 0; i < _ndisp; ++i)
  {
    traction(i) = evaluation.traction(i);
    for (unsigned int j = 0; j < _ndisp; ++j)
      tangent(i, j) = evaluation.jump_tangent(i, j);
  }

  // Materials run before the stabilization prepass of each assembly, so a stabilized interface
  // leaves its state to recordTrialState()
  if (!_stabilized)
    recordState(law, elem_datum, qp, jump);
}

template <typename DatumType>
KOKKOS_FUNCTION Moose::Kokkos::SCZM::BiLinearMixedModeCohesiveLaw
KokkosBiLinearMixedModeTraction::trialLaw(DatumType & elem_datum, const unsigned int qp) const
{
  Moose::Kokkos::SCZM::BiLinearMixedModeCohesiveLaw law;
  law.penalty_stiffness = _penalty_stiffness;
  law.eta = _eta;
  law.viscosity = _viscosity;
  law.criterion = _criterion;
  law.lag_mode_mixity = _lag_mode_mixity;
  law.lag_displacement_jump = _lag_displacement_jump;
  law.alpha = _alpha;

  law.old_state.damage = _damage_old(elem_datum, qp);
  const auto history_jump_old = _history_jump_old(elem_datum, qp);
  for (unsigned int i = 0; i < _ndisp; ++i)
    law.old_state.interface_displacement_jump(i) = history_jump_old(i);

  law.point_inputs.normal_strength = _normal_strength(elem_datum, qp);
  law.point_inputs.shear_strength = _shear_strength(elem_datum, qp);
  law.point_inputs.GI_c = _GI_c(elem_datum, qp);
  law.point_inputs.GII_c = _GII_c(elem_datum, qp);
  law.point_inputs.dt = _dt;
  return law;
}
