//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "gtest/gtest.h"

#include "SCZMSalehaniIraniCohesiveLaw.h"

#include <algorithm>
#include <cmath>

using namespace Moose::Kokkos;
using namespace Moose::Kokkos::SCZM;

namespace
{
SalehaniIraniCohesiveLaw
law()
{
  SalehaniIraniCohesiveLaw result;
  result.normal_gap_at_maximum_normal_traction = 0.8;
  result.tangential_gap_at_maximum_shear_traction = 0.6;
  result.maximum_normal_traction = 2.5;
  result.maximum_shear_traction = 1.7;
  return result;
}
} // namespace

TEST(SCZMSalehaniIraniCohesiveLawTest, PeakTractionsAndInitialStiffness)
{
  const auto evaluator = law();
  EXPECT_FALSE(evaluator.isAffine());
  // Values are O(1-10), so 1e-13 is a few ulps of the exponential and square root
  const Real tol = 1e-13;

  // At zero jump the tangent is diagonal: sigma_max e / delta_n normal and
  // tau_max sqrt(2e) / (sqrt(2) delta_t) = tau_max sqrt(e) / delta_t tangential
  const auto at_zero = evaluator.evaluate(Real3{}, 3);
  ASSERT_TRUE(at_zero.valid);
  EXPECT_NEAR(at_zero.jump_tangent.a[0][0], 2.5 * std::exp(1.0) / 0.8, tol);
  for (const auto i : {1, 2})
  {
    EXPECT_NEAR(at_zero.jump_tangent.a[i][i], 1.7 * std::sqrt(std::exp(1.0)) / 0.6, tol);
    EXPECT_NEAR(at_zero.jump_tangent.a[0][i], 0.0, tol);
    EXPECT_NEAR(at_zero.jump_tangent.a[i][0], 0.0, tol);
  }

  // Pure opening at delta_n gives the maximum normal traction sigma_max e exp(-1), a stationary
  // point of the normal traction
  Real3 opening{};
  opening.v[0] = 0.8;
  const auto at_opening = evaluator.evaluate(opening, 3);
  EXPECT_NEAR(at_opening.traction.v[0], 2.5, tol);
  EXPECT_NEAR(at_opening.jump_tangent.a[0][0], 0.0, tol);

  // Pure shear at delta_t has normalized jump 1 / sqrt(2) and exponent 1 / 2, giving the maximum
  // shear traction tau_max sqrt(2e) / sqrt(2) exp(-1 / 2)
  for (const auto i : {1, 2})
  {
    Real3 shear{};
    shear.v[i] = 0.6;
    const auto at_shear = evaluator.evaluate(shear, 3);
    EXPECT_NEAR(at_shear.traction.v[i], 1.7, tol);
    EXPECT_NEAR(at_shear.jump_tangent.a[i][i], 0.0, tol);
  }
}

TEST(SCZMSalehaniIraniCohesiveLawTest, TangentMatchesCentralDifference)
{
  const auto evaluator = law();
  for (const unsigned int ndisp : {2u, 3u})
  {
    Real3 jump{};
    jump.v[0] = -0.13;
    jump.v[1] = 0.21;
    jump.v[2] = -0.09;
    const auto evaluation = evaluator.evaluate(jump, ndisp);

    for (unsigned int j = 0; j < ndisp; ++j)
    {
      const Real h = 1e-7 * std::max(1.0, std::abs(jump.v[j]));
      Real3 plus = jump;
      Real3 minus = jump;
      plus.v[j] += h;
      minus.v[j] -= h;
      const auto plus_evaluation = evaluator.evaluate(plus, ndisp);
      const auto minus_evaluation = evaluator.evaluate(minus, ndisp);
      for (unsigned int i = 0; i < ndisp; ++i)
      {
        const Real finite_difference =
            (plus_evaluation.traction.v[i] - minus_evaluation.traction.v[i]) / (2.0 * h);
        EXPECT_NEAR(evaluation.jump_tangent.a[i][j],
                    finite_difference,
                    1e-6 * std::max(1.0, std::abs(finite_difference)));
      }
    }
  }
}

TEST(SCZMSalehaniIraniCohesiveLawTest, RejectsNonpositiveParameters)
{
  auto evaluator = law();
  Real3 jump{};
  jump.v[0] = 0.1;

  evaluator.normal_gap_at_maximum_normal_traction = 0.0;
  EXPECT_FALSE(evaluator.evaluate(jump, 3).valid);
  evaluator = law();
  evaluator.tangential_gap_at_maximum_shear_traction = -1.0;
  EXPECT_FALSE(evaluator.evaluate(jump, 3).valid);
  evaluator = law();
  evaluator.maximum_normal_traction = 0.0;
  EXPECT_FALSE(evaluator.evaluate(jump, 3).valid);
  evaluator = law();
  evaluator.maximum_shear_traction = -1.0;
  EXPECT_FALSE(evaluator.evaluate(jump, 3).valid);
}

TEST(SCZMSalehaniIraniCohesiveLawTest, GeneralizedRobinSolve)
{
  const auto evaluator = law();
  Real3 shifted_jump{};
  shifted_jump.v[0] = 0.12;
  shifted_jump.v[1] = -0.07;
  shifted_jump.v[2] = 0.04;
  Real3 bulk_traction{};
  bulk_traction.v[0] = 0.3;
  bulk_traction.v[1] = -0.15;
  bulk_traction.v[2] = 0.08;
  const Real beta = 20.0;

  const auto result =
      GeneralizedRobinSolver::solve(evaluator, shifted_jump, bulk_traction, beta, 3, false);
  ASSERT_TRUE(result.converged);

  const auto evaluation = evaluator.evaluate(result.auxiliary_jump, 3);
  Real residual_squared_norm = 0.0;
  Real right_hand_side_squared_norm = 0.0;
  for (unsigned int i = 0; i < 3; ++i)
  {
    const Real right_hand_side = shifted_jump.v[i] + bulk_traction.v[i] / beta;
    const Real residual =
        result.auxiliary_jump.v[i] + evaluation.traction.v[i] / beta - right_hand_side;
    residual_squared_norm += residual * residual;
    right_hand_side_squared_norm += right_hand_side * right_hand_side;
  }
  // Newton starts at zero jump, where the traction vanishes, so the initial residual norm is that
  // of the right-hand side and sets the relative part of the solver tolerance.
  const GeneralizedRobinSolverOptions options;
  EXPECT_LT(std::sqrt(residual_squared_norm),
            options.absolute_tolerance +
                options.relative_tolerance * std::sqrt(right_hand_side_squared_norm));
}
