//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "gtest/gtest.h"

#include "SCZMCohesiveLawEvaluators.h"

#include "RankFourTensor.h"

#include <cmath>
#include <iterator>
#include <vector>

using namespace Moose::Kokkos;
using namespace Moose::Kokkos::SCZM;

namespace
{
/**
 * Memoryless isotropic exponential softening law f(j) = k j exp(-||j||^2 / delta^2). Its tangent
 * k exp(-r) (I - 2 j j^T / delta^2), r = ||j||^2 / delta^2, turns negative for r > 1/2, with the
 * most negative eigenvalue -2 k exp(-3/2) ~ -0.446 k at r = 3/2. beta > 0.446 k keeps the Robin
 * tangent I + K / beta positive definite.
 */
struct ExponentialSofteningLaw
{
  Real stiffness = 0.0;
  Real opening_scale = 1.0;

  CohesiveEvaluation evaluate(const Real3 & jump, const unsigned int ndisp) const
  {
    CohesiveEvaluation result;
    Real squared_norm = 0.0;
    for (const auto i : make_range(ndisp))
      squared_norm += jump.v[i] * jump.v[i];
    const Real secant = stiffness * std::exp(-squared_norm / (opening_scale * opening_scale));
    for (const auto i : make_range(ndisp))
    {
      result.traction.v[i] = secant * jump.v[i];
      for (const auto j : make_range(ndisp))
        result.jump_tangent.a[i][j] =
            secant *
            ((i == j ? 1.0 : 0.0) - 2.0 * jump.v[i] * jump.v[j] / (opening_scale * opening_scale));
    }
    return result;
  }
};
} // namespace

TEST(SCZMGeneralizedRobinSolverTest, AffineAnisotropicWithOffset)
{
  AffineCohesiveLaw law;
  law.stiffness.a[0][0] = 12.0;
  law.stiffness.a[0][1] = 2.0;
  law.stiffness.a[1][0] = 2.0;
  law.stiffness.a[1][1] = 5.0;
  law.offset.v[0] = 0.3;
  law.offset.v[1] = -0.2;

  Real3 jump{};
  jump.v[0] = 0.1;
  jump.v[1] = -0.04;
  Real3 bulk_traction{};
  bulk_traction.v[0] = 0.8;
  bulk_traction.v[1] = -0.1;
  const Real beta = 7.0;

  const auto result = GeneralizedRobinSolver::solve(law, jump, bulk_traction, beta, 2, true);
  ASSERT_TRUE(result.converged);
  EXPECT_EQ(result.iterations, 1);

  Real33 system{};
  Real3 right_hand_side{};
  for (unsigned int i = 0; i < 2; ++i)
  {
    right_hand_side.v[i] = beta * jump.v[i] + bulk_traction.v[i] - law.offset.v[i];
    for (unsigned int j = 0; j < 2; ++j)
      system.a[i][j] = law.stiffness.a[i][j] + (i == j ? beta : 0.0);
  }
  const Real determinant = system.a[0][0] * system.a[1][1] - system.a[0][1] * system.a[1][0];
  Real3 expected_jump{};
  expected_jump.v[0] =
      (system.a[1][1] * right_hand_side.v[0] - system.a[0][1] * right_hand_side.v[1]) / determinant;
  expected_jump.v[1] =
      (-system.a[1][0] * right_hand_side.v[0] + system.a[0][0] * right_hand_side.v[1]) /
      determinant;
  const auto expected = law.evaluate(expected_jump, 2);

  for (unsigned int i = 0; i < 2; ++i)
  {
    EXPECT_NEAR(result.auxiliary_jump.v[i], expected_jump.v[i], 1e-13);
    EXPECT_NEAR(result.traction.v[i], expected.traction.v[i], 1e-13);
  }
}

TEST(SCZMGeneralizedRobinSolverTest, CoupledNonlinearDerivativesAndCap)
{
  CoupledCubicCohesiveLaw law;
  law.linear_stiffness = 2.0;
  law.cubic_stiffness = 40.0;

  Real3 jump{};
  jump.v[0] = 0.6;
  jump.v[1] = -0.35;
  jump.v[2] = 0.2;
  Real3 bulk_traction{};
  bulk_traction.v[0] = 0.4;
  bulk_traction.v[1] = -0.1;
  bulk_traction.v[2] = 0.3;
  const Real beta = 5.0;

  const auto result = GeneralizedRobinSolver::solve(law, jump, bulk_traction, beta, 3, false);
  ASSERT_TRUE(result.converged);
  EXPECT_GT(result.iterations, 0);

  const Real epsilon = 1e-7;
  for (unsigned int column = 0; column < 3; ++column)
  {
    Real3 perturbed_jump = jump;
    perturbed_jump.v[column] += epsilon;
    const auto jump_result =
        GeneralizedRobinSolver::solve(law, perturbed_jump, bulk_traction, beta, 3, false);
    ASSERT_TRUE(jump_result.converged);

    Real3 perturbed_traction = bulk_traction;
    perturbed_traction.v[column] += epsilon;
    const auto traction_result =
        GeneralizedRobinSolver::solve(law, jump, perturbed_traction, beta, 3, false);
    ASSERT_TRUE(traction_result.converged);

    for (unsigned int row = 0; row < 3; ++row)
    {
      EXPECT_NEAR((jump_result.traction.v[row] - result.traction.v[row]) / epsilon,
                  result.dtraction_djump.a[row][column],
                  2e-6);
      EXPECT_NEAR((traction_result.traction.v[row] - result.traction.v[row]) / epsilon,
                  result.dtraction_dbulk_traction.a[row][column],
                  2e-6);
    }
  }

  // Symmetric PSD K implies symmetric W with eigenvalues in [0, beta]. Gershgorin is too weak for
  // this coupled matrix, so check the Rayleigh quotient on representative directions.
  const Real3 directions[] = {Real3{{1.0, 0.0, 0.0}},
                              Real3{{0.0, 1.0, 0.0}},
                              Real3{{0.0, 0.0, 1.0}},
                              Real3{{1.0, 1.0, 1.0}},
                              Real3{{1.0, -2.0, 0.5}}};
  for (const auto & direction : directions)
  {
    Real numerator = 0.0;
    Real denominator = 0.0;
    for (unsigned int i = 0; i < 3; ++i)
    {
      denominator += direction.v[i] * direction.v[i];
      for (unsigned int j = 0; j < 3; ++j)
        numerator += direction.v[i] * result.dtraction_djump.a[i][j] * direction.v[j];
    }
    EXPECT_GE(numerator / denominator, -1e-12);
    EXPECT_LE(numerator / denominator, beta + 1e-12);
  }
}

TEST(SCZMGeneralizedRobinSolverTest, LargeBetaTangentKeepsDigits)
{
  // beta >> |K| is the penalty-like limit where W = beta (I - S) would cancel about
  // log10(beta / |K|) digits; W = S K must stay accurate to rounding.
  AffineCohesiveLaw law;
  const Real stiffness[3][3] = {{12.0, 2.0, 0.0}, {2.0, 5.0, 1.0}, {0.0, 1.0, 3.0}};
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      law.stiffness.a[i][j] = stiffness[i][j];

  Real3 jump{};
  jump.v[0] = 0.01;
  jump.v[1] = -0.02;
  jump.v[2] = 0.005;
  const Real beta = 1e12;

  const auto result = GeneralizedRobinSolver::solve(law, jump, Real3{}, beta, 3, true);
  ASSERT_TRUE(result.converged);

  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
    {
      // S = I - K / beta + O(|K|^2 / beta^2), so W = K - K K / beta with an O(1e-22) remainder.
      Real expected = stiffness[i][j];
      for (unsigned int k = 0; k < 3; ++k)
        expected -= stiffness[i][k] * stiffness[k][j] / beta;
      // 1e-13 is a few hundred ulps of |K| = 12; the cancelling form errs by about 1e-4.
      EXPECT_NEAR(result.dtraction_djump.a[i][j], expected, 1e-13);
      EXPECT_NEAR(result.dtraction_dbulk_traction.a[i][j] * beta, expected, 1e-13);
    }
}

TEST(SCZMGeneralizedRobinSolverTest, PivotedRobinTangent)
{
  // K = [[-2, 1], [1, 0]] with beta = 2 gives A = I + K / beta = [[0, 0.5], [0.5, 1]], whose zero
  // leading entry requires a row exchange.
  AffineCohesiveLaw law;
  law.stiffness.a[0][0] = -2.0;
  law.stiffness.a[0][1] = 1.0;
  law.stiffness.a[1][0] = 1.0;
  law.offset.v[0] = 0.1;
  law.offset.v[1] = -0.3;
  const Real beta = 2.0;

  Real3 jump{};
  jump.v[0] = 0.2;
  jump.v[1] = 0.4;
  Real3 bulk_traction{};
  bulk_traction.v[0] = -0.5;
  bulk_traction.v[1] = 0.7;

  const auto result = GeneralizedRobinSolver::solve(law, jump, bulk_traction, beta, 2, true);
  ASSERT_TRUE(result.converged);

  const auto evaluation = law.evaluate(result.auxiliary_jump, 2);
  for (unsigned int i = 0; i < 2; ++i)
  {
    EXPECT_NEAR(result.auxiliary_jump.v[i] + evaluation.traction.v[i] / beta,
                jump.v[i] + bulk_traction.v[i] / beta,
                1e-14);
    EXPECT_NEAR(result.traction.v[i], evaluation.traction.v[i], 1e-14);
    for (unsigned int j = 0; j < 2; ++j)
    {
      // A W = K and A S = I.
      Real robin_times_tangent = 0.0;
      Real robin_times_inverse = 0.0;
      for (unsigned int k = 0; k < 2; ++k)
      {
        const Real robin = (i == k ? 1.0 : 0.0) + law.stiffness.a[i][k] / beta;
        robin_times_tangent += robin * result.dtraction_djump.a[k][j];
        robin_times_inverse += robin * result.inverse_robin_tangent.a[k][j];
      }
      EXPECT_NEAR(robin_times_tangent, law.stiffness.a[i][j], 1e-14);
      EXPECT_NEAR(robin_times_inverse, i == j ? 1.0 : 0.0, 1e-14);
    }
  }
}

TEST(SCZMGeneralizedRobinSolverTest, SofteningSlopeIsPreserved)
{
  // A softening slope k = -1 with beta = 4 keeps 1 + k / beta = 3/4 > 0, so the Robin equation
  // has the unique solution j = (0.2 + 0.1 / 4) / (3/4) = 0.3 and W = k / (3/4) = -4/3.
  AffineCohesiveLaw law;
  law.stiffness.a[0][0] = -1.0;
  Real3 jump{};
  jump.v[0] = 0.2;
  Real3 bulk_traction{};
  bulk_traction.v[0] = 0.1;
  const Real beta = 4.0;

  const auto result = GeneralizedRobinSolver::solve(law, jump, bulk_traction, beta, 1, true);
  ASSERT_TRUE(result.converged);
  EXPECT_NEAR(result.auxiliary_jump.v[0], 0.3, 1e-15);
  EXPECT_NEAR(result.traction.v[0], -0.3, 1e-15);
  EXPECT_NEAR(result.dtraction_djump.a[0][0], -4.0 / 3.0, 1e-15);
  EXPECT_NEAR(result.dtraction_dbulk_traction.a[0][0], -1.0 / 3.0, 1e-15);
}

TEST(SCZMGeneralizedRobinSolverTest, RejectsSingularRobinTangent)
{
  AffineCohesiveLaw law;
  law.stiffness.a[0][0] = -3.0;
  Real3 jump{};
  jump.v[0] = 1.0;
  const auto result = GeneralizedRobinSolver::solve(law, jump, Real3{}, 3.0, 1, true);
  EXPECT_FALSE(result.converged);
}
TEST(SCZMGeneralizedRobinSolverTest, RejectsNearlySingularRobinTangent)
{
  // k = -beta (1 - 1e-14) leaves 1 + k / beta = 1e-14: I and K / beta cancel to below
  // minimum_pivot relative to their O(1) scale, so the Robin solution is not trustworthy.
  AffineCohesiveLaw law;
  const Real beta = 3.0;
  law.stiffness.a[0][0] = -beta * (1.0 - 1e-14);
  Real3 jump{};
  jump.v[0] = 1.0;
  EXPECT_FALSE(GeneralizedRobinSolver::solve(law, jump, Real3{}, beta, 1, true).converged);
  // Same cancellation in one direction of a 2D diagonal tangent.
  law.stiffness.a[1][1] = 4.0;
  EXPECT_FALSE(GeneralizedRobinSolver::solve(law, jump, Real3{}, beta, 2, true).converged);
}

namespace
{
using PointStatus = GeneralizedRobinSolver::PointStatus;

void
expectIdentical(const StabilizedTractionResult & actual, const StabilizedTractionResult & expected)
{
  EXPECT_EQ(actual.converged, expected.converged);
  EXPECT_EQ(actual.iterations, expected.iterations);
  for (const auto i : make_range(3))
  {
    EXPECT_EQ(actual.auxiliary_jump.v[i], expected.auxiliary_jump.v[i]);
    EXPECT_EQ(actual.traction.v[i], expected.traction.v[i]);
    for (const auto j : make_range(3))
    {
      EXPECT_EQ(actual.inverse_robin_tangent.a[i][j], expected.inverse_robin_tangent.a[i][j]);
      EXPECT_EQ(actual.dtraction_djump.a[i][j], expected.dtraction_djump.a[i][j]);
      EXPECT_EQ(actual.dtraction_dbulk_traction.a[i][j], expected.dtraction_dbulk_traction.a[i][j]);
    }
  }
}

/// Drives begin()/consume() as a batch caller would and counts the requested evaluations.
template <typename Law>
StabilizedTractionResult
solvePointwise(const Law & law,
               const Real3 & shifted_jump,
               const Real3 & bulk_traction,
               const Real beta,
               const unsigned int ndisp,
               const bool affine,
               unsigned int & evaluations)
{
  auto state = GeneralizedRobinSolver::begin(shifted_jump, bulk_traction, beta, ndisp, affine);
  evaluations = 0;
  while (state.status == PointStatus::Active)
  {
    ++evaluations;
    const auto status =
        GeneralizedRobinSolver::consume(state, law.evaluate(state.requested_jump, ndisp));
    EXPECT_EQ(status, state.status);
  }
  EXPECT_EQ(state.status == PointStatus::Converged, state.result.converged);
  return state.result;
}
} // namespace

TEST(SCZMGeneralizedRobinSolverTest, ResumableIterationMatchesSolve)
{
  AffineCohesiveLaw affine;
  affine.stiffness.a[0][0] = 12.0;
  affine.stiffness.a[0][1] = 2.0;
  affine.stiffness.a[1][0] = 2.0;
  affine.stiffness.a[1][1] = 5.0;
  affine.offset.v[0] = 0.3;
  affine.offset.v[1] = -0.2;
  CoupledCubicCohesiveLaw cubic;
  cubic.linear_stiffness = 2.0;
  cubic.cubic_stiffness = 40.0;
  // beta = 5 > 0.446 k keeps the softening Robin tangent positive definite.
  ExponentialSofteningLaw softening;
  softening.stiffness = 10.0;
  softening.opening_scale = 0.5;

  const Real3 small_jump{{0.1, -0.04, 0.0}};
  const Real3 small_traction{{0.8, -0.1, 0.0}};
  const Real3 large_jump{{0.6, -0.35, 0.2}};
  const Real3 large_traction{{0.4, -0.1, 0.3}};
  const Real3 softening_jump{{0.3, 0.58, -0.81}};
  const Real3 softening_traction{{-0.9, 0.7, -0.1}};
  const Real3 restart_jump{{0.52, -1.0, -0.11}};
  const Real3 restart_traction{{0.4, -0.5, 0.9}};
  const Real3 failing_jump{{0.81, -0.22, 0.14}};
  const Real3 failing_traction{{1.0, -0.9, -0.6}};
  unsigned int evaluations = 0;

  // The affine path requests the initial and the updated jump only.
  auto result = solvePointwise(affine, small_jump, small_traction, 7.0, 2, true, evaluations);
  ASSERT_TRUE(result.converged);
  EXPECT_EQ(evaluations, 2);
  expectIdentical(result,
                  GeneralizedRobinSolver::solve(affine, small_jump, small_traction, 7.0, 2, true));

  result = solvePointwise(cubic, large_jump, large_traction, 5.0, 3, false, evaluations);
  ASSERT_TRUE(result.converged);
  expectIdentical(result,
                  GeneralizedRobinSolver::solve(cubic, large_jump, large_traction, 5.0, 3, false));

  // Without backtracking each iterate costs one evaluation plus the initial one, so more requests
  // show that the line search rejected trial iterates on the softening branch.
  result =
      solvePointwise(softening, softening_jump, softening_traction, 5.0, 3, false, evaluations);
  ASSERT_TRUE(result.converged);
  EXPECT_GT(evaluations, result.iterations + 1);
  expectIdentical(
      result,
      GeneralizedRobinSolver::solve(softening, softening_jump, softening_traction, 5.0, 3, false));

  // beta = 2 < 0.446 k admits a non-monotone Robin operator. Here the line search fails from zero
  // jump and the iteration restarted at the shifted jump converges.
  result = solvePointwise(softening, restart_jump, restart_traction, 2.0, 3, false, evaluations);
  ASSERT_TRUE(result.converged);
  expectIdentical(
      result,
      GeneralizedRobinSolver::solve(softening, restart_jump, restart_traction, 2.0, 3, false));

  // Here the line search fails from both starts.
  result = solvePointwise(softening, failing_jump, failing_traction, 2.0, 3, false, evaluations);
  EXPECT_FALSE(result.converged);
  expectIdentical(
      result,
      GeneralizedRobinSolver::solve(softening, failing_jump, failing_traction, 2.0, 3, false));
}

TEST(SCZMGeneralizedRobinSolverTest, UndamagedRootOfNonmonotoneRobinOperator)
{
  // k / beta = 10 makes j + f(j) / beta non-monotone, with a local maximum 2.513 at j = 0.384 and a
  // local minimum 1.177 at j = 1.042, so the right-hand side 1.5 has the roots 0.148, 0.765, and
  // 1.498. Newton started at the shifted jump 1.5 converges to the root 1.498 past the peak traction
  // at j = 0.5 / sqrt(2) = 0.354, whereas the undamaged root 0.148 is the one reached by loading
  // from zero.
  ExponentialSofteningLaw law;
  law.stiffness = 10.0;
  law.opening_scale = 0.5;
  const Real beta = 1.0;
  Real3 shifted_jump{};
  shifted_jump.v[0] = 1.5;

  const auto result = GeneralizedRobinSolver::solve(law, shifted_jump, Real3{}, beta, 1, false);
  ASSERT_TRUE(result.converged);
  const Real jump = result.auxiliary_jump.v[0];
  EXPECT_LT(jump, 0.5 / std::sqrt(2.0));
  // The initial residual at zero jump is the right-hand side 1.5, which scales the tolerance
  const GeneralizedRobinSolverOptions options;
  EXPECT_NEAR(jump + law.evaluate(result.auxiliary_jump, 1).traction.v[0] / beta,
              1.5,
              options.absolute_tolerance + options.relative_tolerance * 1.5);
}

TEST(SCZMGeneralizedRobinSolverTest, BatchFreezesTerminalPoints)
{
  ExponentialSofteningLaw law;
  law.stiffness = 10.0;
  law.opening_scale = 0.5;

  struct Point
  {
    Real3 shifted_jump;
    Real3 bulk_traction;
    Real beta;
  };
  // The settled point has its Robin solution at zero jump, where Newton starts. Of the beta = 2
  // points the first converges only after the restart at the shifted jump and the second fails
  // from both starts. beta = 0 is rejected by begin().
  const Point points[] = {{Real3{{0.3, -0.2, 0.1}}, Real3{{-1.5, 1.0, -0.5}}, 5.0},
                          {Real3{{0.1, -0.04, 0.0}}, Real3{{0.8, -0.1, 0.0}}, 20.0},
                          {Real3{{0.6, 0.1, 0.0}}, Real3{}, 5.0},
                          {Real3{{0.52, -1.0, -0.11}}, Real3{{0.4, -0.5, 0.9}}, 2.0},
                          {Real3{{0.81, -0.22, 0.14}}, Real3{{1.0, -0.9, -0.6}}, 2.0},
                          {Real3{{0.6, 0.1, 0.0}}, Real3{}, 0.0}};
  const auto n_points = std::size(points);

  std::vector<GeneralizedRobinSolver::PointState> states;
  for (const auto & point : points)
    states.push_back(GeneralizedRobinSolver::begin(
        point.shifted_jump, point.bulk_traction, point.beta, 3, false));
  EXPECT_EQ(states.back().status, PointStatus::Failed);

  // Every round evaluates the whole batch, as one tensor evaluation would, and hands every row to
  // consume(); terminal rows must ignore it. finish_round records when each row became terminal.
  std::vector<GeneralizedRobinSolver::PointState> frozen(n_points);
  std::vector<unsigned int> finish_round(n_points, 0);
  for (const auto p : make_range(n_points))
    if (states[p].status != PointStatus::Active)
      frozen[p] = states[p];

  unsigned int round = 0;
  // The default budget allows at most 1 + 20 * (8 + 1) = 181 evaluations per start (initial
  // iterate, then up to 9 trial iterates for each of 20 iterations), and there are two starts; the
  // cap only guards against a driver that never terminates.
  const unsigned int max_rounds = 400;
  auto any_active = [&states]()
  {
    for (const auto & state : states)
      if (state.status == PointStatus::Active)
        return true;
    return false;
  };
  while (any_active() && round < max_rounds)
  {
    ++round;
    for (const auto p : make_range(n_points))
    {
      const bool was_active = states[p].status == PointStatus::Active;
      const auto evaluation = law.evaluate(states[p].requested_jump, 3);
      GeneralizedRobinSolver::consume(states[p], evaluation);
      if (!was_active)
      {
        EXPECT_EQ(states[p].status, frozen[p].status);
        for (const auto i : make_range(3))
          EXPECT_EQ(states[p].requested_jump.v[i], frozen[p].requested_jump.v[i]);
        expectIdentical(states[p].result, frozen[p].result);
      }
      else if (states[p].status != PointStatus::Active)
      {
        frozen[p] = states[p];
        finish_round[p] = round;
      }
    }
  }
  ASSERT_FALSE(any_active());

  const PointStatus expected_status[] = {PointStatus::Converged,
                                         PointStatus::Converged,
                                         PointStatus::Converged,
                                         PointStatus::Converged,
                                         PointStatus::Failed,
                                         PointStatus::Failed};
  for (const auto p : make_range(n_points))
  {
    EXPECT_EQ(states[p].status, expected_status[p]);
    // A terminal row keeps requesting its last accepted jump.
    for (const auto i : make_range(3))
      EXPECT_EQ(states[p].requested_jump.v[i], states[p].result.auxiliary_jump.v[i]);
    expectIdentical(
        states[p].result,
        GeneralizedRobinSolver::solve(
            law, points[p].shifted_jump, points[p].bulk_traction, points[p].beta, 3, false));
  }
  EXPECT_EQ(finish_round[0], 1);
  EXPECT_EQ(states[0].result.iterations, 0);
  EXPECT_EQ(finish_round[5], 0);
  // The rows converge at different rounds, so the earlier ones stayed frozen above.
  EXPECT_LT(finish_round[1], finish_round[2]);
}

TEST(SCZMGeneralizedRobinSolverTest, NormalStiffnessAndAutomaticStabilization)
{
  // Lame parameters of E = 100, nu = 0.3, whose P-wave modulus lambda + 2 mu is 134.615...
  const Real lambda = 100.0 * 0.3 / (1.3 * 0.4);
  const Real mu = 100.0 / 2.6;
  RankFourTensor tangent;
  tangent.fillSymmetricIsotropic(lambda, mu);

  // The isotropic contraction is independent of the unit normal direction in 2D and 3D. The
  // tolerance is a few ulps of the up-to-81-term sum relative to the moduli.
  const RealVectorValue normal_2d(0.6, 0.8, 0.0);
  const RealVectorValue normal_3d(2.0 / 7.0, -3.0 / 7.0, 6.0 / 7.0);
  EXPECT_NEAR(normalStiffness(tangent, normal_2d, 2), lambda + 2.0 * mu, 1e-12 * mu);
  EXPECT_NEAR(normalStiffness(tangent, normal_3d, 3), lambda + 2.0 * mu, 1e-12 * mu);
  // Entries along the third axis do not enter a 2D contraction
  EXPECT_NEAR(normalStiffness(tangent, RealVectorValue(0.0, 0.0, 1.0), 2), 0.0, 1e-12 * mu);

  // gamma = 5, |face| = 0.5, a_p = 100 * 0.5 / 0.25 = 200, a_n = 300 * 0.5 / 0.5 = 300:
  // w_p = 300 / 500 = 0.6 and beta = 2 * 5 * 200 * 300 / 500 = 1200
  const auto parameters = automaticStabilization(5.0, 0.5, 100.0, 0.25, 300.0, 0.5);
  EXPECT_DOUBLE_EQ(parameters.primary_weight, 0.6);
  EXPECT_DOUBLE_EQ(parameters.beta, 1200.0);
  // Equal sides weight the tractions equally, and beta = gamma a = 5 * 200
  const auto equal = automaticStabilization(5.0, 0.5, 100.0, 0.25, 100.0, 0.25);
  EXPECT_DOUBLE_EQ(equal.primary_weight, 0.5);
  EXPECT_DOUBLE_EQ(equal.beta, 1000.0);
}
