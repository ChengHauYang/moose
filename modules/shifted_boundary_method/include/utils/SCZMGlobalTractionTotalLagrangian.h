//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosCohesiveZoneModelTools.h"
#include "SCZMGeneralizedRobinSolver.h"

namespace Moose::Kokkos::SCZM
{
struct GlobalTractionEvaluation
{
  CohesiveEvaluation cohesive;
  Real3 cauchy_traction{};
  Real dtraction_dF[3][3][3] = {{{0.0}}};
};

/** Applies the total-Lagrangian local-to-global rotation and Nanson area transformation. */
template <typename LocalLaw>
struct GlobalTractionTotalLagrangian
{
  LocalLaw local_law;
  Real33 total_rotation{};
  Real33 reference_rotation{};
  Real33 rotation{};
  Real33 deformation_gradient{};
  Real3 reference_normal{};
  unsigned int ndisp = 0;

  KOKKOS_FUNCTION CohesiveEvaluation evaluate(const Real3 & global_jump,
                                              const unsigned int active_components) const
  {
    return toGlobal(
        local_law.evaluate(toLocalJump(global_jump, active_components), active_components),
        active_components);
  }

  KOKKOS_FUNCTION GlobalTractionEvaluation evaluateWithDeformationGradientDerivative(
      const Real3 & global_jump, const unsigned int active_components) const
  {
    return toGlobalWithDF(
        global_jump,
        local_law.evaluate(toLocalJump(global_jump, active_components), active_components),
        active_components);
  }

  /// Local jump R^T global_jump at which the local law is evaluated
  KOKKOS_FUNCTION Real3 toLocalJump(const Real3 & global_jump,
                                    const unsigned int active_components) const
  {
    Real3 local_jump{};
    for (unsigned int i = 0; i < active_components; ++i)
      for (unsigned int j = 0; j < active_components; ++j)
        local_jump(i) += total_rotation(j, i) * global_jump(j);
    return local_jump;
  }

  /// Global PK1 traction and jump tangent from a local-law evaluation at toLocalJump()
  KOKKOS_FUNCTION CohesiveEvaluation toGlobal(const CohesiveEvaluation & local,
                                              const unsigned int active_components) const
  {
    CohesiveEvaluation result;
    result.valid = local.valid;
    if (!local.valid)
      return result;

    const Real area_ratio = deformation_gradient.determinant(3) *
                            (deformation_gradient.inverse(3).transpose() * reference_normal).norm();
    for (unsigned int i = 0; i < active_components; ++i)
    {
      for (unsigned int j = 0; j < active_components; ++j)
        result.traction(i) += area_ratio * total_rotation(i, j) * local.traction(j);
      for (unsigned int j = 0; j < active_components; ++j)
        for (unsigned int k = 0; k < active_components; ++k)
          for (unsigned int l = 0; l < active_components; ++l)
            result.jump_tangent(i, j) +=
                area_ratio * total_rotation(i, k) * local.jump_tangent(k, l) * total_rotation(j, l);
    }
    return result;
  }

  /**
   * toGlobal() plus the Cauchy traction and the PK1 traction derivative with respect to F, from a
   * local-law evaluation at toLocalJump(global_jump).
   */
  KOKKOS_FUNCTION GlobalTractionEvaluation
  toGlobalWithDF(const Real3 & global_jump,
                 const CohesiveEvaluation & local,
                 const unsigned int active_components) const
  {
    GlobalTractionEvaluation result;
    result.cohesive.valid = local.valid;
    if (!local.valid)
      return result;

    const Real J = deformation_gradient.determinant(3);
    const auto inverse_transpose = deformation_gradient.inverse(3).transpose();
    const Real area_ratio = J * (inverse_transpose * reference_normal).norm();

    for (unsigned int i = 0; i < active_components; ++i)
    {
      for (unsigned int j = 0; j < active_components; ++j)
        result.cauchy_traction(i) += total_rotation(i, j) * local.traction(j);
      result.cohesive.traction(i) = area_ratio * result.cauchy_traction(i);

      for (unsigned int j = 0; j < active_components; ++j)
        for (unsigned int k = 0; k < active_components; ++k)
          for (unsigned int l = 0; l < active_components; ++l)
            result.cohesive.jump_tangent(i, j) +=
                area_ratio * total_rotation(i, k) * local.jump_tangent(k, l) * total_rotation(j, l);
    }

    Real d_inverse[3][3][3][3];
    CohesiveZoneModelTools::computedFinversedF(inverse_transpose, d_inverse);
    Real d_area_dF[3][3];
    CohesiveZoneModelTools::computeDAreaRatioDF(
        inverse_transpose, reference_normal, J, d_inverse, d_area_dF);

    const Real33 stretch = rotation.transpose() * deformation_gradient;
    Real d_rotation_dF[3][3][3][3];
    CohesiveZoneModelTools::computedRdF(rotation, stretch, d_rotation_dF);
    Real d_total_rotation_dF[3][3][3][3] = {{{{0.0}}}};
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
        for (unsigned int k = 0; k < 3; ++k)
          for (unsigned int l = 0; l < 3; ++l)
            for (unsigned int m = 0; m < 3; ++m)
              d_total_rotation_dF[i][j][k][l] +=
                  reference_rotation(m, j) * d_rotation_dF[i][m][k][l];

    Real d_local_jump_dF[3][3][3] = {{{0.0}}};
    for (unsigned int a = 0; a < 3; ++a)
      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int l = 0; l < 3; ++l)
          for (unsigned int b = 0; b < 3; ++b)
            d_local_jump_dF[a][k][l] += d_total_rotation_dF[b][a][k][l] * global_jump(b);

    Real rotated_tangent[3][3] = {{0.0}};
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
        for (unsigned int k = 0; k < 3; ++k)
          rotated_tangent[i][j] += total_rotation(i, k) * local.jump_tangent(k, j);

    for (unsigned int i = 0; i < active_components; ++i)
      for (unsigned int k = 0; k < active_components; ++k)
        for (unsigned int l = 0; l < active_components; ++l)
        {
          Real rotation_term = 0.0;
          Real jump_term = 0.0;
          for (unsigned int j = 0; j < 3; ++j)
          {
            rotation_term += d_total_rotation_dF[i][j][k][l] * local.traction(j);
            jump_term += rotated_tangent[i][j] * d_local_jump_dF[j][k][l];
          }
          result.dtraction_dF[i][k][l] = d_area_dF[k][l] * result.cauchy_traction(i) +
                                         area_ratio * (rotation_term + jump_term);
        }
    return result;
  }
};
} // namespace Moose::Kokkos::SCZM
