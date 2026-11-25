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
#include <cmath>
#define SCZM_KOKKOS_FUNCTION inline
#else
#define SCZM_KOKKOS_FUNCTION KOKKOS_FUNCTION
#endif

namespace Moose::Kokkos::SCZM
{
/** Memoryless Salehani-Irani 3DC cohesive law. */
struct SalehaniIraniCohesiveLaw
{
  Real normal_gap_at_maximum_normal_traction = 0.0;
  Real tangential_gap_at_maximum_shear_traction = 0.0;
  Real maximum_normal_traction = 0.0;
  Real maximum_shear_traction = 0.0;

  SCZM_KOKKOS_FUNCTION bool isAffine() const { return false; }

  SCZM_KOKKOS_FUNCTION CohesiveEvaluation evaluate(const Real3 & local_jump,
                                                   const unsigned int ndisp) const
  {
    CohesiveEvaluation result;
    result.valid = normal_gap_at_maximum_normal_traction > 0.0 &&
                   tangential_gap_at_maximum_shear_traction > 0.0 &&
                   maximum_normal_traction > 0.0 && maximum_shear_traction > 0.0;
    if (!result.valid)
      return result;

    const Real e = exponential(1.0);
    const Real shear_gap = squareRoot(2.0) * tangential_gap_at_maximum_shear_traction;
    const Real shear_factor = squareRoot(2.0 * e);

    Real exponent = local_jump.v[0] / normal_gap_at_maximum_normal_traction;
    for (unsigned int i = 1; i < ndisp; ++i)
    {
      const Real normalized_jump = local_jump.v[i] / shear_gap;
      exponent += normalized_jump * normalized_jump;
    }
    const Real exponential_factor = exponential(-exponent);

    for (unsigned int i = 0; i < ndisp; ++i)
    {
      const Real gap = i == 0 ? normal_gap_at_maximum_normal_traction : shear_gap;
      const Real traction_scale =
          (i == 0 ? maximum_normal_traction * e : maximum_shear_traction * shear_factor);
      const Real normalized_jump = local_jump.v[i] / gap;
      result.traction.v[i] = traction_scale * normalized_jump * exponential_factor;

      for (unsigned int j = 0; j < ndisp; ++j)
      {
        const Real derivative_normalized_jump = i == j ? 1.0 / gap : 0.0;
        const Real exponent_derivative = j == 0 ? 1.0 / normal_gap_at_maximum_normal_traction
                                                : 2.0 * local_jump.v[j] / (shear_gap * shear_gap);
        result.jump_tangent.a[i][j] =
            traction_scale * exponential_factor *
            (derivative_normalized_jump - normalized_jump * exponent_derivative);
      }
    }

    return result;
  }

private:
  SCZM_KOKKOS_FUNCTION static Real exponential(const Real value)
  {
#ifdef MOOSE_KOKKOS_SCOPE
    return ::Kokkos::exp(value);
#else
    return std::exp(value);
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
};
} // namespace Moose::Kokkos::SCZM

#undef SCZM_KOKKOS_FUNCTION
