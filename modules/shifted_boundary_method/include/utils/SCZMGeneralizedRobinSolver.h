//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosTypes.h"

#ifndef MOOSE_KOKKOS_SCOPE
#include <algorithm>
#include <cmath>
#define SCZM_KOKKOS_FUNCTION inline
#else
#define SCZM_KOKKOS_FUNCTION KOKKOS_INLINE_FUNCTION
#endif

namespace Moose::Kokkos::SCZM
{
struct CohesiveEvaluation
{
  Real3 traction{};
  Real33 jump_tangent{};
  bool valid = true;
};

struct StabilizedTractionResult
{
  Real3 traction{};
  Real3 auxiliary_jump{};
  Real33 inverse_robin_tangent{};
  Real33 dtraction_djump{};
  Real33 dtraction_dbulk_traction{};
  unsigned int iterations = 0;
  bool converged = false;
};

struct GeneralizedRobinSolverOptions
{
  unsigned int max_iterations = 20;
  unsigned int max_backtracks = 8;
  Real absolute_tolerance = 1e-12;
  Real relative_tolerance = 1e-10;
  Real minimum_pivot = 1e-12;
};

/**
 * Solve j + f(j) / beta = shifted_jump + bulk_traction / beta at one quadrature point.
 *
 * Evaluator must provide `CohesiveEvaluation evaluate(const Real3 &, unsigned int) const`. Only the
 * first ndisp entries are used. An affine evaluator takes one Newton update and therefore preserves
 * the analytical fast path without a local iteration loop.
 *
 * The Newton iteration is also exposed as a resumable per-point state machine (begin()/consume())
 * for cohesive laws that can only be evaluated for a whole batch of points at once. The caller
 * evaluates the law at PointState::requested_jump and passes the evaluation to consume() until the
 * status is terminal; solve() is that loop for a pointwise evaluator.
 */
class GeneralizedRobinSolver
{
public:
  enum class PointStatus
  {
    Active,
    Converged,
    Failed
  };

  /// Resumable Newton state of one point; terminal points are never modified by consume().
  struct PointState
  {
    /// The step the next evaluation is consumed by
    enum class Stage
    {
      /// Evaluation at the accepted iterate: convergence check and Newton update
      Newton,
      /// Evaluation at a damped trial iterate
      LineSearch,
      /// Evaluation at the single affine update, which must satisfy the tolerance
      AffineCheck
    };

    /// Jump at which the caller evaluates the cohesive law next; frozen once terminal
    Real3 requested_jump{};
    /// Accepted iterate in auxiliary_jump; complete result once status is Converged
    StabilizedTractionResult result{};
    PointStatus status = PointStatus::Failed;
    Stage stage = Stage::Newton;

    Real3 right_hand_side{};
    /// Newton update of the accepted iterate
    Real3 update{};
    /// Residual norm of the accepted iterate
    Real residual_norm = 0.0;
    /// Convergence tolerance, fixed by the residual norm of the initial iterate
    Real tolerance = 0.0;
    /// Line-search multiplier of the current trial iterate
    Real damping = 1.0;
    unsigned int iteration = 0;
    unsigned int backtrack = 0;

    /// Shifted jump, the start of the second attempt
    Real3 shifted_jump{};
    /// Whether the iteration was restarted from the shifted jump
    bool restarted = false;

    Real beta = 0.0;
    unsigned int ndisp = 0;
    bool affine = false;
    GeneralizedRobinSolverOptions options{};
  };

  template <typename Evaluator>
  SCZM_KOKKOS_FUNCTION static StabilizedTractionResult
  solve(const Evaluator & evaluator,
        const Real3 & shifted_jump,
        const Real3 & bulk_traction,
        const Real beta,
        const unsigned int ndisp,
        const bool affine,
        const GeneralizedRobinSolverOptions & options = {})
  {
    auto state = begin(shifted_jump, bulk_traction, beta, ndisp, affine, options);
    while (state.status == PointStatus::Active)
      consume(state, evaluator.evaluate(state.requested_jump, ndisp));
    return state.result;
  }

  /**
   * Starts the Newton iteration at zero jump; Failed for inadmissible beta or ndisp.
   *
   * For a softening law whose tangent exceeds beta in magnitude, j + f(j) / beta is not monotone
   * and the Robin equation has a root on the undamaged branch and one past the peak traction. Since
   * beta is much smaller than the initial cohesive stiffness, the shifted jump can lie past the
   * peak even when the undamaged root is far below it, so Newton is started at zero to reach the
   * undamaged root, and restarted at the shifted jump only when no undamaged root is found.
   */
  SCZM_KOKKOS_FUNCTION static PointState begin(const Real3 & shifted_jump,
                                               const Real3 & bulk_traction,
                                               const Real beta,
                                               const unsigned int ndisp,
                                               const bool affine,
                                               const GeneralizedRobinSolverOptions & options = {})
  {
    PointState state;
    state.shifted_jump = shifted_jump;
    state.beta = beta;
    state.ndisp = ndisp;
    state.affine = affine;
    state.options = options;
    if (!(beta > 0.0) || ndisp < 1 || ndisp > 3 || !finite(beta) || options.max_iterations == 0)
      return state;

    for (unsigned int i = 0; i < ndisp; ++i)
      state.right_hand_side.v[i] = shifted_jump.v[i] + bulk_traction.v[i] / beta;
    state.status = PointStatus::Active;
    return state;
  }

  /// Advances an active point with the cohesive evaluation at its requested jump.
  SCZM_KOKKOS_FUNCTION static PointStatus consume(PointState & state,
                                                  const CohesiveEvaluation & evaluation)
  {
    if (state.status != PointStatus::Active)
      return state.status;

    switch (state.stage)
    {
      case PointState::Stage::Newton:
        newtonStep(state, evaluation);
        break;
      case PointState::Stage::LineSearch:
        lineSearchStep(state, evaluation);
        break;
      case PointState::Stage::AffineCheck:
        affineCheck(state, evaluation);
        break;
    }

    if (state.status == PointStatus::Failed && !state.restarted)
      restart(state);
    if (state.status != PointStatus::Active)
      state.requested_jump = state.result.auxiliary_jump;
    return state.status;
  }

private:
  /// Restarts a failed iteration from the shifted jump.
  SCZM_KOKKOS_FUNCTION static void restart(PointState & state)
  {
    state.result = StabilizedTractionResult{};
    state.result.auxiliary_jump = state.shifted_jump;
    state.requested_jump = state.shifted_jump;
    state.stage = PointState::Stage::Newton;
    state.iteration = 0;
    state.backtrack = 0;
    state.restarted = true;
    state.status = PointStatus::Active;
  }

  /// Robin residual r = j + f(j) / beta - (shifted_jump + bulk_traction / beta).
  SCZM_KOKKOS_FUNCTION static Real3
  robinResidual(const PointState & state, const Real3 & jump, const CohesiveEvaluation & evaluation)
  {
    Real3 residual{};
    for (unsigned int i = 0; i < state.ndisp; ++i)
      residual.v[i] =
          jump.v[i] + evaluation.traction.v[i] / state.beta - state.right_hand_side.v[i];
    return residual;
  }

  SCZM_KOKKOS_FUNCTION static void finish(PointState & state, const CohesiveEvaluation & evaluation)
  {
    state.result = consistentResult(
        state.result, evaluation, state.beta, state.ndisp, state.options.minimum_pivot);
    state.status = state.result.converged ? PointStatus::Converged : PointStatus::Failed;
  }

  /// Convergence check and Newton update at the accepted iterate result.auxiliary_jump.
  SCZM_KOKKOS_FUNCTION static void newtonStep(PointState & state,
                                              const CohesiveEvaluation & evaluation)
  {
    const auto ndisp = state.ndisp;
    if (!validEvaluation(evaluation, ndisp))
    {
      state.status = PointStatus::Failed;
      return;
    }

    const auto residual = robinResidual(state, state.result.auxiliary_jump, evaluation);
    state.residual_norm = norm(residual, ndisp);
    if (!finite(state.residual_norm))
    {
      state.status = PointStatus::Failed;
      return;
    }
    if (state.iteration == 0)
      state.tolerance =
          state.options.absolute_tolerance + state.options.relative_tolerance * state.residual_norm;

    if (state.residual_norm <= state.tolerance)
    {
      state.result.iterations = state.iteration;
      finish(state, evaluation);
      return;
    }

    Real33 robin_tangent{};
    const Real pivot_tolerance =
        robinTangent(evaluation, state.beta, ndisp, state.options.minimum_pivot, robin_tangent);

    for (unsigned int i = 0; i < ndisp; ++i)
      state.update.v[i] = -residual.v[i];
    if (!solveDense(robin_tangent, state.update, ndisp, pivot_tolerance))
    {
      state.status = PointStatus::Failed;
      return;
    }

    if (state.affine)
    {
      for (unsigned int i = 0; i < ndisp; ++i)
        state.result.auxiliary_jump.v[i] += state.update.v[i];
      state.result.iterations = 1;
      state.requested_jump = state.result.auxiliary_jump;
      state.stage = PointState::Stage::AffineCheck;
      return;
    }

    state.damping = 1.0;
    state.backtrack = 0;
    for (unsigned int i = 0; i < ndisp; ++i)
      state.requested_jump.v[i] =
          state.result.auxiliary_jump.v[i] + state.damping * state.update.v[i];
    state.stage = PointState::Stage::LineSearch;
  }

  /**
   * Accepts the trial iterate when it is admissible and decreases the residual norm, otherwise
   * halves the damping. The accepted trial evaluation is the evaluation at the next iterate, so it
   * is reused for that iterate's Newton step instead of being requested again.
   */
  SCZM_KOKKOS_FUNCTION static void lineSearchStep(PointState & state,
                                                  const CohesiveEvaluation & evaluation)
  {
    const auto ndisp = state.ndisp;
    if (validEvaluation(evaluation, ndisp) &&
        norm(robinResidual(state, state.requested_jump, evaluation), ndisp) < state.residual_norm)
    {
      state.result.auxiliary_jump = state.requested_jump;
      ++state.iteration;
      if (state.iteration >= state.options.max_iterations)
      {
        state.status = PointStatus::Failed;
        return;
      }
      state.stage = PointState::Stage::Newton;
      newtonStep(state, evaluation);
      return;
    }

    state.damping *= 0.5;
    ++state.backtrack;
    if (state.backtrack > state.options.max_backtracks)
    {
      state.status = PointStatus::Failed;
      return;
    }
    for (unsigned int i = 0; i < ndisp; ++i)
      state.requested_jump.v[i] =
          state.result.auxiliary_jump.v[i] + state.damping * state.update.v[i];
  }

  /// Accepts the affine update only when it satisfies the tolerance of the initial iterate.
  SCZM_KOKKOS_FUNCTION static void affineCheck(PointState & state,
                                               const CohesiveEvaluation & evaluation)
  {
    if (!validEvaluation(evaluation, state.ndisp) ||
        norm(robinResidual(state, state.result.auxiliary_jump, evaluation), state.ndisp) >
            state.tolerance)
    {
      state.status = PointStatus::Failed;
      return;
    }
    finish(state, evaluation);
  }

  SCZM_KOKKOS_FUNCTION static bool finite(const Real value)
  {
#ifdef MOOSE_KOKKOS_SCOPE
    return ::Kokkos::isfinite(value);
#else
    return std::isfinite(value);
#endif
  }

  SCZM_KOKKOS_FUNCTION static Real absolute(const Real value)
  {
#ifdef MOOSE_KOKKOS_SCOPE
    return ::Kokkos::abs(value);
#else
    return std::abs(value);
#endif
  }

  SCZM_KOKKOS_FUNCTION static Real maximum(const Real first, const Real second)
  {
#ifdef MOOSE_KOKKOS_SCOPE
    return ::Kokkos::max(first, second);
#else
    return std::max(first, second);
#endif
  }

  SCZM_KOKKOS_FUNCTION static Real squareRoot(const Real value)
  {
#ifdef MOOSE_KOKKOS_SCOPE
    return ::Kokkos::sqrt(value);
#else
    return std::sqrt(value);
#endif
  }

  SCZM_KOKKOS_FUNCTION static Real norm(const Real3 & value, const unsigned int ndisp)
  {
    Real squared_norm = 0.0;
    for (unsigned int i = 0; i < ndisp; ++i)
      squared_norm += value.v[i] * value.v[i];
    return squareRoot(squared_norm);
  }

  SCZM_KOKKOS_FUNCTION static bool validEvaluation(const CohesiveEvaluation & evaluation,
                                                   const unsigned int ndisp)
  {
    if (!evaluation.valid)
      return false;
    for (unsigned int i = 0; i < ndisp; ++i)
    {
      if (!finite(evaluation.traction.v[i]))
        return false;
      for (unsigned int j = 0; j < ndisp; ++j)
        if (!finite(evaluation.jump_tangent.a[i][j]))
          return false;
    }
    return true;
  }

  /**
   * Builds the Robin tangent A = I + K / beta and returns the pivot tolerance for factoring it.
   * A is singular only when I and K / beta cancel, so pivots are measured against the magnitude
   * max(1, max |K_ij| / beta) of those summands rather than against the cancelled entries, which
   * would accept any nonzero 1x1 or diagonal A.
   */
  SCZM_KOKKOS_FUNCTION static Real robinTangent(const CohesiveEvaluation & evaluation,
                                                const Real beta,
                                                const unsigned int ndisp,
                                                const Real minimum_pivot,
                                                Real33 & robin_tangent)
  {
    Real scale = 1.0;
    for (unsigned int i = 0; i < ndisp; ++i)
      for (unsigned int j = 0; j < ndisp; ++j)
      {
        robin_tangent.a[i][j] = (i == j ? 1.0 : 0.0) + evaluation.jump_tangent.a[i][j] / beta;
        scale = maximum(scale, absolute(evaluation.jump_tangent.a[i][j]) / beta);
      }
    return minimum_pivot * scale;
  }

  /// LU factorization with partial pivoting; fails when a pivot is not above pivot_tolerance.
  SCZM_KOKKOS_FUNCTION static bool factorDense(Real33 & matrix,
                                               unsigned int permutation[3],
                                               const unsigned int ndisp,
                                               const Real pivot_tolerance)
  {
    for (unsigned int row = 0; row < ndisp; ++row)
      permutation[row] = row;
    for (unsigned int column = 0; column < ndisp; ++column)
    {
      unsigned int pivot_row = column;
      Real pivot_magnitude = absolute(matrix.a[column][column]);
      for (unsigned int row = column + 1; row < ndisp; ++row)
        if (absolute(matrix.a[row][column]) > pivot_magnitude)
        {
          pivot_row = row;
          pivot_magnitude = absolute(matrix.a[row][column]);
        }

      if (!finite(pivot_magnitude) || pivot_magnitude <= pivot_tolerance)
        return false;

      if (pivot_row != column)
      {
        for (unsigned int entry = 0; entry < ndisp; ++entry)
        {
          const Real temporary = matrix.a[column][entry];
          matrix.a[column][entry] = matrix.a[pivot_row][entry];
          matrix.a[pivot_row][entry] = temporary;
        }
        const auto temporary = permutation[column];
        permutation[column] = permutation[pivot_row];
        permutation[pivot_row] = temporary;
      }

      for (unsigned int row = column + 1; row < ndisp; ++row)
      {
        matrix.a[row][column] /= matrix.a[column][column];
        for (unsigned int entry = column + 1; entry < ndisp; ++entry)
          matrix.a[row][entry] -= matrix.a[row][column] * matrix.a[column][entry];
      }
    }
    return true;
  }

  SCZM_KOKKOS_FUNCTION static bool solveFactored(const Real33 & matrix,
                                                 const unsigned int permutation[3],
                                                 Real3 & right_hand_side,
                                                 const unsigned int ndisp)
  {
    Real3 permuted{};
    for (unsigned int row = 0; row < ndisp; ++row)
      permuted.v[row] = right_hand_side.v[permutation[row]];
    for (unsigned int row = 0; row < ndisp; ++row)
      for (unsigned int column = 0; column < row; ++column)
        permuted.v[row] -= matrix.a[row][column] * permuted.v[column];
    for (int row = static_cast<int>(ndisp) - 1; row >= 0; --row)
    {
      for (unsigned int column = row + 1; column < ndisp; ++column)
        permuted.v[row] -= matrix.a[row][column] * permuted.v[column];
      permuted.v[row] /= matrix.a[row][row];
      if (!finite(permuted.v[row]))
        return false;
    }
    right_hand_side = permuted;
    return true;
  }

  SCZM_KOKKOS_FUNCTION static bool solveDense(Real33 matrix,
                                              Real3 & right_hand_side,
                                              const unsigned int ndisp,
                                              const Real pivot_tolerance)
  {
    unsigned int permutation[3]{};
    return factorDense(matrix, permutation, ndisp, pivot_tolerance) &&
           solveFactored(matrix, permutation, right_hand_side, ndisp);
  }

  SCZM_KOKKOS_FUNCTION static StabilizedTractionResult
  consistentResult(StabilizedTractionResult result,
                   const CohesiveEvaluation & evaluation,
                   const Real beta,
                   const unsigned int ndisp,
                   const Real minimum_pivot)
  {
    result.traction = evaluation.traction;
    Real33 robin_tangent{};
    const Real pivot_tolerance =
        robinTangent(evaluation, beta, ndisp, minimum_pivot, robin_tangent);

    unsigned int permutation[3]{};
    if (!factorDense(robin_tangent, permutation, ndisp, pivot_tolerance))
      return result;

    for (unsigned int column = 0; column < ndisp; ++column)
    {
      Real3 inverse_column{};
      inverse_column.v[column] = 1.0;
      if (!solveFactored(robin_tangent, permutation, inverse_column, ndisp))
        return result;
      for (unsigned int row = 0; row < ndisp; ++row)
        result.inverse_robin_tangent.a[row][column] = inverse_column.v[row];
    }

    for (unsigned int row = 0; row < ndisp; ++row)
      for (unsigned int column = 0; column < ndisp; ++column)
      {
        for (unsigned int inner = 0; inner < ndisp; ++inner)
          result.dtraction_djump.a[row][column] +=
              result.inverse_robin_tangent.a[row][inner] * evaluation.jump_tangent.a[inner][column];
        result.dtraction_dbulk_traction.a[row][column] =
            result.dtraction_djump.a[row][column] / beta;
      }

    result.converged = true;
    return result;
  }
};

/**
 * Normal stiffness n_i n_J n_k n_L C_iJkL of a bulk tangent C, which is the same contraction for
 * dsigma/deps and dP/dF. For an isotropic elastic C it is the P-wave modulus lambda + 2 mu.
 */
template <typename Tangent, typename Vector>
SCZM_KOKKOS_FUNCTION Real
normalStiffness(const Tangent & tangent, const Vector & normal, const unsigned int ndisp)
{
  Real stiffness = 0.0;
  for (unsigned int i = 0; i < ndisp; ++i)
    for (unsigned int J = 0; J < ndisp; ++J)
      for (unsigned int k = 0; k < ndisp; ++k)
        for (unsigned int L = 0; L < ndisp; ++L)
          stiffness += normal(i) * normal(J) * normal(k) * normal(L) * tangent(i, J, k, L);
  return stiffness;
}

/// Weights of the average bulk traction and the Robin parameter of one interface quadrature point
struct StabilizationParameters
{
  /// Weight of the primary side traction in T_h; the neighbor side traction has 1 - primary_weight
  Real primary_weight = 0.5;
  Real beta = 0.0;
};

/**
 * Automatic stabilization parameters from the normal stiffness k and volume |V| of the elements on
 * both sides of an interface face. |V| / |face| is the element size normal to the face, so
 * a = k |face| / |V| is the bulk stiffness per unit area. Each side's traction is weighted by the
 * other side's stiffness, w_p = a_n / (a_p + a_n) and w_n = a_p / (a_p + a_n), and
 *
 *   beta = gamma * 2 a_p a_n / (a_p + a_n),
 *
 * the harmonic mean of a scaled by the dimensionless gamma. A trace inequality bounds the traction
 * of each side by its own a, so the average needs beta of order sum_s w_s^2 a_s, which is half the
 * harmonic mean for these weights; equal weights would need half the arithmetic mean. The weights
 * thus keep beta at the scale of the compliant side, and the stabilization effective, when the two
 * sides differ in stiffness by orders of magnitude (Annavarapu, Hautefeuille, and Dolbow, Comput.
 * Methods Appl. Mech. Eng. 225-228, 2012). Equal sides give equal weights and beta = gamma a.
 */
SCZM_KOKKOS_FUNCTION StabilizationParameters
automaticStabilization(const Real gamma,
                       const Real face_area,
                       const Real primary_stiffness,
                       const Real primary_volume,
                       const Real neighbor_stiffness,
                       const Real neighbor_volume)
{
  const Real primary = primary_stiffness * face_area / primary_volume;
  const Real neighbor = neighbor_stiffness * face_area / neighbor_volume;
  const Real sum = primary + neighbor;
  StabilizationParameters parameters;
  parameters.primary_weight = neighbor / sum;
  parameters.beta = 2.0 * gamma * primary * neighbor / sum;
  return parameters;
}
} // namespace Moose::Kokkos::SCZM

#undef SCZM_KOKKOS_FUNCTION
