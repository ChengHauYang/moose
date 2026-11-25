//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "SCZMGeneralizedRobinSolver.h"

#ifndef MOOSE_KOKKOS_SCOPE
#include <algorithm>
#include <cmath>
#define SCZM_KOKKOS_FUNCTION inline
#else
#define SCZM_KOKKOS_FUNCTION KOKKOS_FUNCTION
#endif

namespace Moose::Kokkos::SCZM
{
/**
 * Device-callable port of BiLinearMixedModeTraction::computeTrialTraction for generalized Robin
 * evaluation in the local interface frame (component 0 = normal).
 */
struct BiLinearMixedModeCohesiveLaw
{
  enum class MixedModeCriterion
  {
    POWER_LAW,
    BK
  };

  /// Committed stateful and lagged history values fixed during a trial solve.
  struct OldState
  {
    Real damage = 0.0;
    Real3 interface_displacement_jump{};
  };

  /// Point-wise material strengths, fracture energies, and time step set per QP.
  struct PointInputs
  {
    Real normal_strength = 0.0;
    Real shear_strength = 0.0;
    Real GI_c = 0.0;
    Real GII_c = 0.0;
    Real dt = 0.0;
  };

  Real penalty_stiffness = 0.0;
  Real eta = 0.0;
  Real viscosity = 0.0;
  MixedModeCriterion criterion = MixedModeCriterion::BK;
  bool lag_mode_mixity = true;
  bool lag_displacement_jump = false;
  // Matches BiLinearMixedModeTraction's default regularization length for the Macaulay bracket.
  Real alpha = 1e-10;

  OldState old_state{};
  PointInputs point_inputs{};

  SCZM_KOKKOS_FUNCTION bool isAffine() const { return false; }

  SCZM_KOKKOS_FUNCTION CohesiveEvaluation evaluate(const Real3 & local_jump,
                                                   unsigned int ndisp) const;

  SCZM_KOKKOS_FUNCTION Real candidateDamage(const Real3 & local_jump, unsigned int ndisp) const;

private:
  struct StateEvaluation
  {
    CohesiveEvaluation cohesive;
    Real damage = 0.0;
  };

  SCZM_KOKKOS_FUNCTION StateEvaluation evaluateState(const Real3 & local_jump,
                                                     unsigned int ndisp) const;

  SCZM_KOKKOS_FUNCTION static bool isFinite(Real value);
  SCZM_KOKKOS_FUNCTION static Real absolute(Real value);
  SCZM_KOKKOS_FUNCTION static Real minimum(Real first, Real second);
  SCZM_KOKKOS_FUNCTION static Real maximum(Real first, Real second);
  SCZM_KOKKOS_FUNCTION static Real squareRoot(Real value);
  SCZM_KOKKOS_FUNCTION static Real power(Real base, Real exponent);
  SCZM_KOKKOS_FUNCTION static Real sine(Real value);
  SCZM_KOKKOS_FUNCTION static Real cosine(Real value);
  SCZM_KOKKOS_FUNCTION static Real regularizedHeavyside(Real value, Real smoothing_length);
  SCZM_KOKKOS_FUNCTION static Real regularizedHeavysideDerivative(Real value,
                                                                  Real smoothing_length);
};

SCZM_KOKKOS_FUNCTION bool
BiLinearMixedModeCohesiveLaw::isFinite(const Real value)
{
#ifdef MOOSE_KOKKOS_SCOPE
  return ::Kokkos::isfinite(value);
#else
  return std::isfinite(value);
#endif
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::absolute(const Real value)
{
#ifdef MOOSE_KOKKOS_SCOPE
  return ::Kokkos::abs(value);
#else
  return std::abs(value);
#endif
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::minimum(const Real first, const Real second)
{
#ifdef MOOSE_KOKKOS_SCOPE
  return ::Kokkos::min(first, second);
#else
  return std::min(first, second);
#endif
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::maximum(const Real first, const Real second)
{
#ifdef MOOSE_KOKKOS_SCOPE
  return ::Kokkos::max(first, second);
#else
  return std::max(first, second);
#endif
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::squareRoot(const Real value)
{
#ifdef MOOSE_KOKKOS_SCOPE
  return ::Kokkos::sqrt(value);
#else
  return std::sqrt(value);
#endif
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::power(const Real base, const Real exponent)
{
#ifdef MOOSE_KOKKOS_SCOPE
  return ::Kokkos::pow(base, exponent);
#else
  return std::pow(base, exponent);
#endif
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::sine(const Real value)
{
#ifdef MOOSE_KOKKOS_SCOPE
  return ::Kokkos::sin(value);
#else
  return std::sin(value);
#endif
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::cosine(const Real value)
{
#ifdef MOOSE_KOKKOS_SCOPE
  return ::Kokkos::cos(value);
#else
  return std::cos(value);
#endif
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::regularizedHeavyside(const Real value, const Real smoothing_length)
{
  // Same C^1 sinusoidal regularization as MathUtils::regularizedHeavyside.
  constexpr Real pi = 3.14159265358979323846;
  if (value <= -smoothing_length)
    return 0.0;
  if (value < smoothing_length)
    return 0.5 * (1.0 + sine(0.5 * pi * value / smoothing_length));
  return 1.0;
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::regularizedHeavysideDerivative(const Real value,
                                                             const Real smoothing_length)
{
  // Same derivative as MathUtils::regularizedHeavysideDerivative.
  constexpr Real pi = 3.14159265358979323846;
  if (value < smoothing_length && value > -smoothing_length)
    return 0.25 * pi / smoothing_length * cosine(0.5 * pi * value / smoothing_length);
  return 0.0;
}

SCZM_KOKKOS_FUNCTION CohesiveEvaluation
BiLinearMixedModeCohesiveLaw::evaluate(const Real3 & local_jump, const unsigned int ndisp) const
{
  return evaluateState(local_jump, ndisp).cohesive;
}

SCZM_KOKKOS_FUNCTION Real
BiLinearMixedModeCohesiveLaw::candidateDamage(const Real3 & local_jump,
                                              const unsigned int ndisp) const
{
  return evaluateState(local_jump, ndisp).damage;
}

SCZM_KOKKOS_FUNCTION BiLinearMixedModeCohesiveLaw::StateEvaluation
BiLinearMixedModeCohesiveLaw::evaluateState(const Real3 & local_jump,
                                            const unsigned int ndisp) const
{
  StateEvaluation state;
  state.cohesive.valid = false;
  state.damage = old_state.damage;

  if (ndisp < 1 || ndisp > 3 || !(penalty_stiffness > 0.0) || !isFinite(penalty_stiffness) ||
      !(point_inputs.normal_strength > 0.0) || !isFinite(point_inputs.normal_strength) ||
      !(point_inputs.shear_strength > 0.0) || !isFinite(point_inputs.shear_strength) ||
      !(point_inputs.GI_c > 0.0) || !isFinite(point_inputs.GI_c) || !(point_inputs.GII_c > 0.0) ||
      !isFinite(point_inputs.GII_c) || !(alpha > 0.0) || !isFinite(alpha) || viscosity < 0.0 ||
      !isFinite(viscosity) || !isFinite(eta) || !isFinite(old_state.damage) ||
      (viscosity > 0.0 && !(point_inputs.dt > 0.0)) ||
      (viscosity > 0.0 && !isFinite(point_inputs.dt)))
    return state;

  Real trial_jump[3] = {0.0, 0.0, 0.0};
  Real old_jump[3] = {0.0, 0.0, 0.0};
  for (unsigned int i = 0; i < ndisp; ++i)
  {
    if (!isFinite(local_jump.v[i]) || !isFinite(old_state.interface_displacement_jump.v[i]))
      return state;
    trial_jump[i] = local_jump.v[i];
    old_jump[i] = old_state.interface_displacement_jump.v[i];
  }

  // Matches MooseUtils::absoluteFuzzyEqual(x, 0.0) default tolerance libMesh::TOLERANCE^2 = 1e-12.
  constexpr Real fuzzy_zero_tol = 1e-12;

  const Real * const mixity_delta = lag_mode_mixity ? old_jump : trial_jump;

  Real beta = 0.0;
  Real dbeta_ddelta[3] = {0.0, 0.0, 0.0};
  if (mixity_delta[0] > 0.0)
  {
    const Real delta_s =
        squareRoot(mixity_delta[1] * mixity_delta[1] + mixity_delta[2] * mixity_delta[2]);
    beta = delta_s / mixity_delta[0];

    if (!lag_mode_mixity && delta_s > 0.0)
    {
      dbeta_ddelta[0] = -delta_s / mixity_delta[0] / mixity_delta[0];
      dbeta_ddelta[1] = mixity_delta[1] / delta_s / mixity_delta[0];
      dbeta_ddelta[2] = mixity_delta[2] / delta_s / mixity_delta[0];
    }
  }

  const Real delta_normal0 = point_inputs.normal_strength / penalty_stiffness;
  const Real delta_shear0 = point_inputs.shear_strength / penalty_stiffness;

  Real delta_init = delta_shear0;
  Real ddelta_init_ddelta[3] = {0.0, 0.0, 0.0};
  if (mixity_delta[0] > 0.0)
  {
    const Real beta_delta_n = beta * delta_normal0;
    const Real delta_mixed = squareRoot(delta_shear0 * delta_shear0 + beta_delta_n * beta_delta_n);
    delta_init = delta_normal0 * delta_shear0 * squareRoot(1.0 + beta * beta) / delta_mixed;
    if (!lag_mode_mixity)
    {
      const Real ratio = delta_normal0 / delta_mixed;
      const Real ddelta_init_dbeta =
          delta_init * beta * (1.0 / (1.0 + beta * beta) - ratio * ratio);
      for (unsigned int i = 0; i < ndisp; ++i)
        ddelta_init_ddelta[i] = ddelta_init_dbeta * dbeta_ddelta[i];
    }
  }

  Real delta_final = 2.0 * point_inputs.GII_c / point_inputs.shear_strength;
  Real ddelta_final_ddelta[3] = {0.0, 0.0, 0.0};
  if (mixity_delta[0] > 0.0)
  {
    const Real beta_sq = beta * beta;
    if (criterion == MixedModeCriterion::BK)
    {
      const Real mix_ratio = beta_sq / (1.0 + beta_sq);
      delta_final =
          2.0 / penalty_stiffness / delta_init *
          (point_inputs.GI_c + (point_inputs.GII_c - point_inputs.GI_c) * power(mix_ratio, eta));
      if (!lag_mode_mixity)
      {
        const Real ddelta_final_ddelta_init = -delta_final / delta_init;
        const Real denom = (1.0 + beta_sq) * (1.0 + beta_sq);
        const Real ddelta_final_dbeta = absolute(beta) <= fuzzy_zero_tol
                                            ? 0.0
                                            : 2.0 / penalty_stiffness / delta_init *
                                                  (point_inputs.GII_c - point_inputs.GI_c) * eta *
                                                  power(mix_ratio, eta - 1.0) * 2.0 * beta / denom;
        for (unsigned int i = 0; i < ndisp; ++i)
          ddelta_final_ddelta[i] = ddelta_final_ddelta_init * ddelta_init_ddelta[i] +
                                   ddelta_final_dbeta * dbeta_ddelta[i];
      }
    }
    else
    {
      const Real inv_eta = 1.0 / eta;
      const Real term1 = power(1.0 / point_inputs.GI_c, eta);
      const Real term2 = power(beta_sq / point_inputs.GII_c, eta);
      const Real Gc_mixed = term1 + term2;
      if (!(Gc_mixed > 0.0) || !isFinite(Gc_mixed))
        return state;

      delta_final =
          (2.0 + 2.0 * beta_sq) / penalty_stiffness / delta_init * power(Gc_mixed, -inv_eta);
      if (!lag_mode_mixity)
      {
        const Real ddelta_final_ddelta_init = -delta_final / delta_init;
        const Real ddelta_final_dbeta =
            absolute(beta) <= fuzzy_zero_tol
                ? 0.0
                : delta_final * 2.0 * beta / (1.0 + beta_sq) -
                      (2.0 + 2.0 * beta_sq) / penalty_stiffness / delta_init *
                          power(Gc_mixed, -inv_eta - 1.0) *
                          power(beta_sq / point_inputs.GII_c, eta - 1.0) * 2.0 * beta /
                          point_inputs.GII_c;
        for (unsigned int i = 0; i < ndisp; ++i)
          ddelta_final_ddelta[i] = ddelta_final_ddelta_init * ddelta_init_ddelta[i] +
                                   ddelta_final_dbeta * dbeta_ddelta[i];
      }
    }
  }

  if (!(delta_final > delta_init) || !isFinite(delta_init) || !isFinite(delta_final))
    return state;

  const Real * const eff_delta = lag_displacement_jump ? old_jump : trial_jump;

  const Real heaviside = regularizedHeavyside(eff_delta[0], alpha);
  const Real delta_normal_pos = heaviside * eff_delta[0];
  const Real delta_m = squareRoot(eff_delta[1] * eff_delta[1] + eff_delta[2] * eff_delta[2] +
                                  delta_normal_pos * delta_normal_pos);
  Real ddelta_m_ddelta[3] = {0.0, 0.0, 0.0};
  if (!lag_displacement_jump && absolute(delta_m) > fuzzy_zero_tol)
  {
    const Real ddelta_normal_pos_ddelta_normal =
        regularizedHeavysideDerivative(eff_delta[0], alpha) * eff_delta[0] + heaviside;
    ddelta_m_ddelta[0] = delta_normal_pos * ddelta_normal_pos_ddelta_normal / delta_m;
    ddelta_m_ddelta[1] = eff_delta[1] / delta_m;
    ddelta_m_ddelta[2] = eff_delta[2] / delta_m;
  }

  Real d = 0.0;
  if (delta_m < delta_init)
    d = 0.0;
  else if (delta_m > delta_final)
    d = 1.0;
  else
    d = delta_final * (delta_m - delta_init) / delta_m / (delta_final - delta_init);

  Real dd_ddelta[3] = {0.0, 0.0, 0.0};
  if (d < old_state.damage)
    // Irreversibility
    d = old_state.damage;
  else if (delta_m >= delta_init && delta_m <= delta_final)
  {
    const Real denom_part = delta_final - delta_init;
    const Real denom_full = delta_m * denom_part;
    const Real inv_denom_sq = 1.0 / (denom_full * denom_full);
    for (unsigned int i = 0; i < ndisp; ++i)
      dd_ddelta[i] = (ddelta_final_ddelta[i] * (delta_m - delta_init) +
                      delta_final * (ddelta_m_ddelta[i] - ddelta_init_ddelta[i])) /
                         denom_full -
                     delta_final * (delta_m - delta_init) *
                         (ddelta_m_ddelta[i] * denom_part +
                          delta_m * (ddelta_final_ddelta[i] - ddelta_init_ddelta[i])) *
                         inv_denom_sq;
  }

  // Viscous regularization
  if (viscosity > 0.0)
  {
    const Real factor = viscosity / point_inputs.dt + 1.0;
    d = (d + viscosity * old_state.damage / point_inputs.dt) / factor;
    for (unsigned int i = 0; i < ndisp; ++i)
      dd_ddelta[i] /= factor;
  }

  // Split displacement jump into active and inactive parts
  const Real delta_active[3] = {maximum(trial_jump[0], 0.0), trial_jump[1], trial_jump[2]};
  const Real delta_inactive_0 = minimum(trial_jump[0], 0.0);

  state.cohesive.traction.v[0] =
      (1.0 - d) * penalty_stiffness * delta_active[0] + penalty_stiffness * delta_inactive_0;
  for (unsigned int i = 1; i < ndisp; ++i)
    state.cohesive.traction.v[i] = (1.0 - d) * penalty_stiffness * delta_active[i];

  const Real dactive_0 = trial_jump[0] > 0.0 ? 1.0 : 0.0;
  const Real dinactive_0 = trial_jump[0] < 0.0 ? 1.0 : 0.0;
  for (unsigned int i = 0; i < ndisp; ++i)
  {
    const Real diag_active = (i == 0 ? dactive_0 : 1.0);
    const Real diag_inactive = (i == 0 ? dinactive_0 : 0.0);
    for (unsigned int j = 0; j < ndisp; ++j)
      state.cohesive.jump_tangent.a[i][j] =
          (i == j
               ? ((1.0 - d) * penalty_stiffness * diag_active + penalty_stiffness * diag_inactive)
               : 0.0) -
          penalty_stiffness * delta_active[i] * dd_ddelta[j];
  }

  state.damage = d;
  state.cohesive.valid = isFinite(d);
  return state;
}
} // namespace Moose::Kokkos::SCZM

#undef SCZM_KOKKOS_FUNCTION
