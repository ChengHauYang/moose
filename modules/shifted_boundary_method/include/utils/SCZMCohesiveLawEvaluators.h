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
#define SCZM_KOKKOS_FUNCTION inline
#else
#define SCZM_KOKKOS_FUNCTION KOKKOS_FUNCTION
#endif

namespace Moose::Kokkos::SCZM
{
/** Memoryless affine cohesive law f(j) = K j + c. */
struct AffineCohesiveLaw
{
  Real33 stiffness{};
  Real3 offset{};

  SCZM_KOKKOS_FUNCTION bool isAffine() const { return true; }

  SCZM_KOKKOS_FUNCTION CohesiveEvaluation evaluate(const Real3 & jump,
                                                   const unsigned int ndisp) const
  {
    CohesiveEvaluation result;
    result.jump_tangent = stiffness;
    result.traction = offset;
    for (unsigned int i = 0; i < ndisp; ++i)
      for (unsigned int j = 0; j < ndisp; ++j)
        result.traction.v[i] += stiffness.a[i][j] * jump.v[j];
    return result;
  }
};

/** Memoryless monotone coupled law f(j) = k0 j + a ||j||^2 j + c. */
struct CoupledCubicCohesiveLaw
{
  Real linear_stiffness = 0.0;
  Real cubic_stiffness = 0.0;
  Real3 offset{};

  SCZM_KOKKOS_FUNCTION bool isAffine() const { return false; }

  SCZM_KOKKOS_FUNCTION CohesiveEvaluation evaluate(const Real3 & jump,
                                                   const unsigned int ndisp) const
  {
    CohesiveEvaluation result;
    Real squared_norm = 0.0;
    for (unsigned int i = 0; i < ndisp; ++i)
      squared_norm += jump.v[i] * jump.v[i];

    const Real isotropic_tangent = linear_stiffness + cubic_stiffness * squared_norm;
    for (unsigned int i = 0; i < ndisp; ++i)
    {
      result.traction.v[i] = offset.v[i] + isotropic_tangent * jump.v[i];
      for (unsigned int j = 0; j < ndisp; ++j)
        result.jump_tangent.a[i][j] =
            (i == j ? isotropic_tangent : 0.0) + 2.0 * cubic_stiffness * jump.v[i] * jump.v[j];
    }
    result.valid = linear_stiffness >= 0.0 && cubic_stiffness >= 0.0;
    return result;
  }
};

/** Device-dispatchable native memoryless cohesive law. */
struct NativeCohesiveLaw
{
  bool coupled_cubic = false;
  AffineCohesiveLaw affine;
  CoupledCubicCohesiveLaw cubic;

  SCZM_KOKKOS_FUNCTION bool isAffine() const { return !coupled_cubic; }

  SCZM_KOKKOS_FUNCTION CohesiveEvaluation evaluate(const Real3 & jump,
                                                   const unsigned int ndisp) const
  {
    return coupled_cubic ? cubic.evaluate(jump, ndisp) : affine.evaluate(jump, ndisp);
  }
};
} // namespace Moose::Kokkos::SCZM

#undef SCZM_KOKKOS_FUNCTION
