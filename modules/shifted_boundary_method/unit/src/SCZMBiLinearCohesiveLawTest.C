//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "gtest/gtest.h"

#include "MathUtils.h"
#include "MooseUtils.h"
#include "SCZMBiLinearCohesiveLaw.h"

#include <algorithm>
#include <cmath>

using namespace Moose::Kokkos;
using namespace Moose::Kokkos::SCZM;

namespace
{
struct HostBiLinearReference
{
  Real K = 1e6;
  Real eta = 2.2;
  Real viscosity = 0.0;
  BiLinearMixedModeCohesiveLaw::MixedModeCriterion criterion =
      BiLinearMixedModeCohesiveLaw::MixedModeCriterion::BK;
  bool lag_mode_mixity = true;
  bool lag_displacement_jump = false;
  Real alpha = 1e-10;

  Real GI_c = 1e3;
  Real GII_c = 1e2;
  Real N = 1e4;
  Real S = 1e3;
  Real dt = 0.1;

  struct Output
  {
    Real3 traction{};
    Real33 tangent{};
    Real damage = 0.0;
  };

  Output evaluate(const Real3 & trial_jump, const Real d_old, const Real3 & jump_old) const
  {
    const Real3 & mixity_delta = lag_mode_mixity ? jump_old : trial_jump;

    Real beta = 0.0;
    Real dbeta_ddelta[3] = {0.0, 0.0, 0.0};
    if (mixity_delta.v[0] > 0.0)
    {
      const Real delta_s =
          std::sqrt(mixity_delta.v[1] * mixity_delta.v[1] + mixity_delta.v[2] * mixity_delta.v[2]);
      beta = delta_s / mixity_delta.v[0];
      if (!lag_mode_mixity && delta_s > 0.0)
      {
        dbeta_ddelta[0] = -delta_s / mixity_delta.v[0] / mixity_delta.v[0];
        dbeta_ddelta[1] = mixity_delta.v[1] / delta_s / mixity_delta.v[0];
        dbeta_ddelta[2] = mixity_delta.v[2] / delta_s / mixity_delta.v[0];
      }
    }

    const Real delta_n0 = N / K;
    const Real delta_s0 = S / K;
    Real delta_init = delta_s0;
    Real ddelta_init_ddelta[3] = {0.0, 0.0, 0.0};
    if (mixity_delta.v[0] > 0.0)
    {
      const Real delta_mixed =
          std::sqrt(delta_s0 * delta_s0 + (beta * delta_n0) * (beta * delta_n0));
      delta_init = delta_n0 * delta_s0 * std::sqrt(1.0 + beta * beta) / delta_mixed;
      if (!lag_mode_mixity)
      {
        const Real ddelta_init_dbeta =
            delta_init * beta *
            (1.0 / (1.0 + beta * beta) - (delta_n0 / delta_mixed) * (delta_n0 / delta_mixed));
        for (const auto i : make_range(3u))
          ddelta_init_ddelta[i] = ddelta_init_dbeta * dbeta_ddelta[i];
      }
    }

    Real delta_final = 2.0 * GII_c / S;
    Real ddelta_final_ddelta[3] = {0.0, 0.0, 0.0};
    if (mixity_delta.v[0] > 0.0)
    {
      if (criterion == BiLinearMixedModeCohesiveLaw::MixedModeCriterion::BK)
      {
        delta_final = 2.0 / K / delta_init *
                      (GI_c + (GII_c - GI_c) * std::pow(beta * beta / (1.0 + beta * beta), eta));
        if (!lag_mode_mixity)
        {
          const Real ddelta_final_ddelta_init = -delta_final / delta_init;
          const Real ddelta_final_dbeta =
              MooseUtils::absoluteFuzzyEqual(beta, 0.0)
                  ? 0.0
                  : 2.0 / K / delta_init * (GII_c - GI_c) * eta *
                        std::pow(beta * beta / (1.0 + beta * beta), eta - 1.0) * 2.0 * beta /
                        ((1.0 + beta * beta) * (1.0 + beta * beta));
          for (const auto i : make_range(3u))
            ddelta_final_ddelta[i] = ddelta_final_ddelta_init * ddelta_init_ddelta[i] +
                                     ddelta_final_dbeta * dbeta_ddelta[i];
        }
      }
      else
      {
        const Real Gc_mixed = std::pow(1.0 / GI_c, eta) + std::pow(beta * beta / GII_c, eta);
        delta_final = (2.0 + 2.0 * beta * beta) / K / delta_init * std::pow(Gc_mixed, -1.0 / eta);
        if (!lag_mode_mixity)
        {
          const Real ddelta_final_ddelta_init = -delta_final / delta_init;
          const Real ddelta_final_dbeta =
              delta_final * 2.0 * beta / (1.0 + beta * beta) -
              (2.0 + 2.0 * beta * beta) / K / delta_init * std::pow(Gc_mixed, -1.0 / eta - 1.0) *
                  std::pow(beta * beta / GII_c, eta - 1.0) * 2.0 * beta / GII_c;
          for (const auto i : make_range(3u))
            ddelta_final_ddelta[i] = ddelta_final_ddelta_init * ddelta_init_ddelta[i] +
                                     ddelta_final_dbeta * dbeta_ddelta[i];
        }
      }
    }

    const Real3 & eff_delta = lag_displacement_jump ? jump_old : trial_jump;
    const Real delta_normal_pos =
        MathUtils::regularizedHeavyside(eff_delta.v[0], alpha) * eff_delta.v[0];
    const Real delta_m =
        std::sqrt(eff_delta.v[1] * eff_delta.v[1] + eff_delta.v[2] * eff_delta.v[2] +
                  delta_normal_pos * delta_normal_pos);
    Real ddelta_m_ddelta[3] = {0.0, 0.0, 0.0};
    if (!lag_displacement_jump && !MooseUtils::absoluteFuzzyEqual(delta_m, 0.0))
    {
      const Real ddn =
          MathUtils::regularizedHeavysideDerivative(eff_delta.v[0], alpha) * eff_delta.v[0] +
          MathUtils::regularizedHeavyside(eff_delta.v[0], alpha);
      ddelta_m_ddelta[0] = delta_normal_pos * ddn / delta_m;
      ddelta_m_ddelta[1] = eff_delta.v[1] / delta_m;
      ddelta_m_ddelta[2] = eff_delta.v[2] / delta_m;
    }

    Real d = 0.0;
    if (delta_m < delta_init)
      d = 0.0;
    else if (delta_m > delta_final)
      d = 1.0;
    else
      d = delta_final * (delta_m - delta_init) / delta_m / (delta_final - delta_init);

    Real dd_ddelta[3] = {0.0, 0.0, 0.0};
    if (d < d_old)
      d = d_old;
    else if (delta_m >= delta_init && delta_m <= delta_final)
      for (const auto i : make_range(3u))
        dd_ddelta[i] = (ddelta_final_ddelta[i] * (delta_m - delta_init) +
                        delta_final * (ddelta_m_ddelta[i] - ddelta_init_ddelta[i])) /
                           delta_m / (delta_final - delta_init) -
                       delta_final * (delta_m - delta_init) *
                           (ddelta_m_ddelta[i] * (delta_final - delta_init) +
                            delta_m * (ddelta_final_ddelta[i] - ddelta_init_ddelta[i])) /
                           std::pow(delta_m * (delta_final - delta_init), 2);

    d = (d + viscosity * d_old / dt) / (viscosity / dt + 1.0);
    for (const auto i : make_range(3u))
      dd_ddelta[i] /= (viscosity / dt + 1.0);

    const Real delta_active[3] = {std::max(trial_jump.v[0], 0.0), trial_jump.v[1], trial_jump.v[2]};
    const Real delta_inactive_0 = std::min(trial_jump.v[0], 0.0);

    Output out;
    out.traction.v[0] = (1.0 - d) * K * delta_active[0] + K * delta_inactive_0;
    out.traction.v[1] = (1.0 - d) * K * delta_active[1];
    out.traction.v[2] = (1.0 - d) * K * delta_active[2];

    const Real da = trial_jump.v[0] > 0.0 ? 1.0 : 0.0;
    const Real di = trial_jump.v[0] < 0.0 ? 1.0 : 0.0;
    for (const auto i : make_range(3u))
      for (const auto j : make_range(3u))
      {
        const Real diag_a = (i == j) ? (i == 0 ? da : 1.0) : 0.0;
        const Real diag_i = (i == j && i == 0) ? di : 0.0;
        out.tangent.a[i][j] =
            (1.0 - d) * K * diag_a + K * diag_i - K * delta_active[i] * dd_ddelta[j];
      }
    out.damage = d;
    return out;
  }

  BiLinearMixedModeCohesiveLaw makeDeviceLaw(const Real d_old, const Real3 & jump_old) const
  {
    BiLinearMixedModeCohesiveLaw law;
    law.penalty_stiffness = K;
    law.eta = eta;
    law.viscosity = viscosity;
    law.criterion = criterion;
    law.lag_mode_mixity = lag_mode_mixity;
    law.lag_displacement_jump = lag_displacement_jump;
    law.alpha = alpha;
    law.old_state = {d_old, jump_old};
    law.point_inputs = {N, S, GI_c, GII_c, dt};
    return law;
  }
};

void
expectMatchesHost(const HostBiLinearReference & ref,
                  const Real3 & trial_jump,
                  const Real d_old,
                  const Real3 & jump_old,
                  const unsigned int ndisp = 3)
{
  const auto law = ref.makeDeviceLaw(d_old, jump_old);
  const auto dev = law.evaluate(trial_jump, ndisp);
  const Real dev_damage = law.candidateDamage(trial_jump, ndisp);
  const auto host = ref.evaluate(trial_jump, d_old, jump_old);

  ASSERT_TRUE(dev.valid);
  // Both sides evaluate the same arithmetic, so the tolerances admit round-off only: about 1e-13
  // relative to tractions up to the strength N = 1e4 and tangents up to K = 1e6
  EXPECT_NEAR(dev_damage, host.damage, 1e-13);
  EXPECT_GE(dev_damage, d_old - 1e-15);

  for (const auto i : make_range(ndisp))
  {
    EXPECT_NEAR(dev.traction.v[i], host.traction.v[i], 1e-9);
    for (const auto j : make_range(ndisp))
      EXPECT_NEAR(dev.jump_tangent.a[i][j], host.tangent.a[i][j], 1e-6);
  }
}

void
expectCentralFDTangent(const BiLinearMixedModeCohesiveLaw & law,
                       const Real3 & jump,
                       const unsigned int ndisp,
                       const Real step = 1e-7,
                       const Real tol = 1e-4)
{
  const auto base = law.evaluate(jump, ndisp);
  ASSERT_TRUE(base.valid);
  for (const auto j : make_range(ndisp))
  {
    Real3 jp = jump;
    Real3 jm = jump;
    jp.v[j] += step;
    jm.v[j] -= step;
    const auto fp = law.evaluate(jp, ndisp);
    const auto fm = law.evaluate(jm, ndisp);
    ASSERT_TRUE(fp.valid);
    ASSERT_TRUE(fm.valid);
    for (const auto i : make_range(ndisp))
    {
      const Real fd = (fp.traction.v[i] - fm.traction.v[i]) / (2.0 * step);
      EXPECT_NEAR(base.jump_tangent.a[i][j], fd, tol) << "i=" << i << " j=" << j;
    }
  }
}
} // namespace

TEST(SCZMBiLinearCohesiveLawTest, ElasticSofteningUnloadingFullDamageAndCompressionMatchHost)
{
  HostBiLinearReference ref;
  ref.lag_mode_mixity = false;
  ref.viscosity = 1e-3;
  ref.dt = 0.05;

  const Real3 zero_old{};
  // Elastic branch: delta_m < delta_init (delta_normal0 = 0.01, delta_shear0 = 0.001)
  expectMatchesHost(ref, Real3{{0.0005, 0.0003, -0.0002}}, 0.0, zero_old);

  // Softening branch with BK criterion and viscosity
  expectMatchesHost(ref, Real3{{0.02, 0.015, -0.01}}, 0.15, Real3{{0.015, 0.01, -0.005}});

  // Softening branch with POWER_LAW and lag_mode_mixity = true (3D and 2D)
  ref.criterion = BiLinearMixedModeCohesiveLaw::MixedModeCriterion::POWER_LAW;
  ref.lag_mode_mixity = true;
  expectMatchesHost(ref, Real3{{0.03, 0.02, 0.01}}, 0.2, Real3{{0.025, 0.015, 0.005}});
  expectMatchesHost(ref, Real3{{0.03, 0.02, 0.0}}, 0.2, Real3{{0.025, 0.015, 0.0}}, 2);

  // Unloading branch: old_state.damage > trial damage
  ref.lag_mode_mixity = false;
  expectMatchesHost(ref, Real3{{0.002, 0.001, 0.0}}, 0.65, Real3{{0.02, 0.01, 0.0}});

  // Fully damaged branch (both without and with viscosity)
  ref.viscosity = 0.0;
  expectMatchesHost(ref, Real3{{0.5, 0.3, -0.2}}, 0.4, Real3{{0.2, 0.1, 0.0}});
  ref.viscosity = 1e-3;
  expectMatchesHost(ref, Real3{{0.5, 0.3, -0.2}}, 0.4, Real3{{0.2, 0.1, 0.0}});

  // Compression + shear branch: normal traction uses full K without damage reduction
  const Real3 comp_jump{{-0.05, 0.02, -0.01}};
  expectMatchesHost(ref, comp_jump, 0.5, Real3{{0.01, 0.01, 0.0}});
  const auto comp_eval = ref.makeDeviceLaw(0.5, Real3{{0.01, 0.01, 0.0}}).evaluate(comp_jump, 3);
  EXPECT_NEAR(comp_eval.traction.v[0], ref.K * comp_jump.v[0], 1e-10);
}

TEST(SCZMBiLinearCohesiveLawTest, CentralFiniteDifferenceTangentInsideSmoothBranches)
{
  HostBiLinearReference ref;
  ref.lag_mode_mixity = false;
  ref.viscosity = 0.02;
  ref.dt = 0.1;

  // BK criterion inside softening window
  ref.criterion = BiLinearMixedModeCohesiveLaw::MixedModeCriterion::BK;
  expectCentralFDTangent(
      ref.makeDeviceLaw(0.1, Real3{{0.02, 0.01, -0.005}}), Real3{{0.025, 0.012, -0.008}}, 3);

  // POWER_LAW criterion inside softening window
  ref.criterion = BiLinearMixedModeCohesiveLaw::MixedModeCriterion::POWER_LAW;
  expectCentralFDTangent(
      ref.makeDeviceLaw(0.1, Real3{{0.02, 0.01, -0.005}}), Real3{{0.025, 0.012, -0.008}}, 3);

  // Lagged mode mixity inside softening window (delta_m still varies with trial jump)
  ref.lag_mode_mixity = true;
  expectCentralFDTangent(
      ref.makeDeviceLaw(0.1, Real3{{0.02, 0.015, 0.005}}), Real3{{0.03, 0.02, 0.008}}, 3);

  // Compression + shear softening branch far outside [-alpha, alpha]
  ref.lag_mode_mixity = false;
  ref.viscosity = 0.0;
  expectCentralFDTangent(ref.makeDeviceLaw(0.1, Real3{}), Real3{{-0.05, 0.03, -0.01}}, 3);
}

TEST(SCZMBiLinearCohesiveLawTest, CandidateDamageIsMonotoneAndConstAndGuardsInvalidInputs)
{
  HostBiLinearReference ref;
  ref.lag_mode_mixity = false;
  ref.viscosity = 0.02;
  ref.dt = 0.1;

  const Real old_d = 0.45;
  const Real3 old_j{{0.02, 0.01, 0.0}};
  const auto law = ref.makeDeviceLaw(old_d, old_j);

  const Real3 trials[] = {Real3{{0.0, 0.0, 0.0}},
                          Real3{{0.0001, 0.0, 0.0}},
                          Real3{{-0.02, 0.001, 0.0}},
                          Real3{{0.01, 0.005, -0.002}},
                          Real3{{0.05, 0.02, 0.01}},
                          Real3{{0.5, 0.0, 0.0}}};
  for (const auto & jump : trials)
  {
    const Real d = law.candidateDamage(jump, 3);
    EXPECT_GE(d, old_d - 1e-15);
    EXPECT_LE(d, 1.0 + 1e-15);
  }
  EXPECT_DOUBLE_EQ(law.old_state.damage, old_d);

  // Invalid domain checks
  auto bad = law;
  bad.penalty_stiffness = 0.0;
  EXPECT_FALSE(bad.evaluate(trials[3], 3).valid);

  bad = law;
  bad.point_inputs.dt = 0.0; // viscosity > 0 requires dt > 0
  EXPECT_FALSE(bad.evaluate(trials[3], 3).valid);

  bad = law;
  bad.alpha = 0.0;
  EXPECT_FALSE(bad.evaluate(trials[3], 3).valid);

  bad = law;
  bad.point_inputs.normal_strength = 1e6; // snap-back: delta_final < delta_init
  bad.point_inputs.GI_c = 1e-3;
  bad.point_inputs.GII_c = 1e-3;
  EXPECT_FALSE(bad.evaluate(trials[3], 3).valid);
}
