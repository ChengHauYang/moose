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

using namespace Moose::Kokkos;
using namespace Moose::Kokkos::SCZM;

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
