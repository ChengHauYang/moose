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
#define SCZM_KOKKOS_FUNCTION KOKKOS_FUNCTION
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
 */
class GeneralizedRobinSolver
{
public:
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
    StabilizedTractionResult result;
    result.auxiliary_jump = shifted_jump;
    if (!(beta > 0.0) || ndisp < 1 || ndisp > 3 || !finite(beta))
      return result;

    Real3 right_hand_side{};
    for (unsigned int i = 0; i < ndisp; ++i)
      right_hand_side.v[i] = shifted_jump.v[i] + bulk_traction.v[i] / beta;

    CohesiveEvaluation evaluation;
    Real initial_norm = 0.0;
    for (unsigned int iteration = 0; iteration < options.max_iterations; ++iteration)
    {
      evaluation = evaluator.evaluate(result.auxiliary_jump, ndisp);
      if (!validEvaluation(evaluation, ndisp))
        return result;

      Real3 residual{};
      for (unsigned int i = 0; i < ndisp; ++i)
        residual.v[i] =
            result.auxiliary_jump.v[i] + evaluation.traction.v[i] / beta - right_hand_side.v[i];
      const Real residual_norm = norm(residual, ndisp);
      if (!finite(residual_norm))
        return result;
      if (iteration == 0)
        initial_norm = residual_norm;

      const Real tolerance = options.absolute_tolerance + options.relative_tolerance * initial_norm;
      if (residual_norm <= tolerance)
      {
        result.iterations = iteration;
        return consistentResult(result, evaluation, beta, ndisp, options.minimum_pivot);
      }

      Real33 robin_tangent{};
      const Real pivot_tolerance =
          robinTangent(evaluation, beta, ndisp, options.minimum_pivot, robin_tangent);

      Real3 update{};
      for (unsigned int i = 0; i < ndisp; ++i)
        update.v[i] = -residual.v[i];
      if (!solveDense(robin_tangent, update, ndisp, pivot_tolerance))
        return result;

      if (affine)
      {
        for (unsigned int i = 0; i < ndisp; ++i)
          result.auxiliary_jump.v[i] += update.v[i];
        result.iterations = 1;
        evaluation = evaluator.evaluate(result.auxiliary_jump, ndisp);
        if (!validEvaluation(evaluation, ndisp))
          return result;
        for (unsigned int i = 0; i < ndisp; ++i)
          residual.v[i] =
              result.auxiliary_jump.v[i] + evaluation.traction.v[i] / beta - right_hand_side.v[i];
        if (norm(residual, ndisp) > tolerance)
          return result;
        return consistentResult(result, evaluation, beta, ndisp, options.minimum_pivot);
      }

      Real damping = 1.0;
      bool accepted = false;
      for (unsigned int backtrack = 0; backtrack <= options.max_backtracks; ++backtrack)
      {
        Real3 trial_jump{};
        for (unsigned int i = 0; i < ndisp; ++i)
          trial_jump.v[i] = result.auxiliary_jump.v[i] + damping * update.v[i];
        const auto trial_evaluation = evaluator.evaluate(trial_jump, ndisp);
        if (validEvaluation(trial_evaluation, ndisp))
        {
          Real3 trial_residual{};
          for (unsigned int i = 0; i < ndisp; ++i)
            trial_residual.v[i] =
                trial_jump.v[i] + trial_evaluation.traction.v[i] / beta - right_hand_side.v[i];
          if (norm(trial_residual, ndisp) < residual_norm)
          {
            result.auxiliary_jump = trial_jump;
            accepted = true;
            break;
          }
        }
        damping *= 0.5;
      }
      if (!accepted)
        return result;
    }

    return result;
  }

private:
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
} // namespace Moose::Kokkos::SCZM

#undef SCZM_KOKKOS_FUNCTION
