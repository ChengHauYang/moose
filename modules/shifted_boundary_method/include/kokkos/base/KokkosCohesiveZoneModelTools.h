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
#include <Kokkos_Core.hpp>
#include <Kokkos_MathematicalFunctions.hpp>

namespace Moose::Kokkos::CohesiveZoneModelTools
{

/**
 * Computes eigenvalues and eigenvectors of a 3x3 symmetric matrix using the classical
 * cyclic Jacobi method.
 *
 * @param C The symmetric 3x3 input matrix
 * @param eigvals Output array of 3 eigenvalues
 * @param V Output orthogonal matrix whose columns are the eigenvectors
 */
KOKKOS_INLINE_FUNCTION void
symmetricEigenvaluesEigenvectors3x3(const Real33 & C, Real eigvals[3], Real33 & V)
{
  Real33 A = C;
  V = 0.0;
  V(0, 0) = 1.0;
  V(1, 1) = 1.0;
  V(2, 2) = 1.0;

  // 15 sweeps is sufficient for double-precision convergence (typically reaches threshold in 3-5
  // sweeps)
  const unsigned int max_sweeps = 15;
  // Threshold below which off-diagonal elements are treated as zero
  const Real eps = 1e-15;

  for (unsigned int sweep = 0; sweep < max_sweeps; ++sweep)
  {
    const Real off = ::Kokkos::fabs(A(0, 1)) + ::Kokkos::fabs(A(0, 2)) + ::Kokkos::fabs(A(1, 2));
    if (off < eps)
      break;

    const unsigned int p_indices[3] = {0, 0, 1};
    const unsigned int q_indices[3] = {1, 2, 2};

    for (unsigned int step = 0; step < 3; ++step)
    {
      const unsigned int p = p_indices[step];
      const unsigned int q = q_indices[step];
      const Real Apq = A(p, q);
      if (::Kokkos::fabs(Apq) < eps)
        continue;

      const Real tau = (A(q, q) - A(p, p)) / (2.0 * Apq);
      const Real t =
          (tau >= 0.0 ? 1.0 : -1.0) / (::Kokkos::fabs(tau) + ::Kokkos::sqrt(tau * tau + 1.0));
      const Real c = 1.0 / ::Kokkos::sqrt(t * t + 1.0);
      const Real s = t * c;

      A(p, p) -= t * Apq;
      A(q, q) += t * Apq;
      A(p, q) = 0.0;
      A(q, p) = 0.0;

      const unsigned int r = 3 - p - q;
      const Real Apr = A(p, r);
      const Real Aqr = A(q, r);
      A(p, r) = c * Apr - s * Aqr;
      A(r, p) = A(p, r);
      A(q, r) = s * Apr + c * Aqr;
      A(r, q) = A(q, r);

      for (unsigned int i = 0; i < 3; ++i)
      {
        const Real vip = V(i, p);
        const Real viq = V(i, q);
        V(i, p) = c * vip - s * viq;
        V(i, q) = s * vip + c * viq;
      }
    }
  }

  for (unsigned int i = 0; i < 3; ++i)
    eigvals[i] = A(i, i);
}

/**
 * Computes polar decomposition F = R * U for a 3x3 deformation gradient.
 *
 * @param F The 3x3 deformation gradient
 * @param R Output 3x3 orthogonal rotation matrix
 * @param U Output 3x3 symmetric right stretch tensor
 * @param Uinv Output 3x3 inverse of right stretch tensor
 */
KOKKOS_INLINE_FUNCTION void
polarDecomposition3x3(const Real33 & F, Real33 & R, Real33 & U, Real33 & Uinv)
{
  const Real33 C = F.transpose() * F;
  Real eigvals[3];
  Real33 V;
  symmetricEigenvaluesEigenvectors3x3(C, eigvals, V);

  U = 0.0;
  Uinv = 0.0;
  for (unsigned int k = 0; k < 3; ++k)
  {
    // Clamp to small positive value to guard against tiny nonpositive numerical noise
    const Real val = eigvals[k] > 0.0 ? eigvals[k] : 1e-30;
    const Real s = ::Kokkos::sqrt(val);
    const Real inv_s = 1.0 / s;
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
      {
        U(i, j) += s * V(i, k) * V(j, k);
        Uinv(i, j) += inv_s * V(i, k) * V(j, k);
      }
  }

  R = F * Uinv;
}

/**
 * Computes reference rotation aligning undeformed normal with the cohesive coordinate system.
 * Reproduces CohesiveZoneModelTools::computeReferenceRotation on device.
 */
KOKKOS_INLINE_FUNCTION Real33
referenceRotation(const Real3 & normal, const unsigned int dim)
{
  Real33 rot = 0.0;

  if (dim == 1)
  {
    rot(0, 0) = 1.0;
    rot(1, 1) = 1.0;
    rot(2, 2) = 1.0;
    return rot;
  }

  if (dim == 2)
  {
    const Real theta = ::Kokkos::atan2(normal(1), normal(0));
    const Real st = ::Kokkos::sin(theta);
    const Real ct = ::Kokkos::cos(theta);
    rot(0, 0) = ct;
    rot(0, 1) = -st;
    rot(1, 0) = st;
    rot(1, 1) = ct;
    rot(2, 2) = 1.0;
    return rot;
  }

  Real3 n = normal;
  const Real n_norm = n.norm();
  n(0) /= n_norm;
  n(1) /= n_norm;
  n(2) /= n_norm;

  const Real wx = ::Kokkos::fabs(n(0));
  const Real wy = ::Kokkos::fabs(n(1));
  const Real wz = ::Kokkos::fabs(n(2));

  Real3 v1 = 0.0;
  if ((wz >= wy && wy >= wx) || (wy >= wz && wz >= wx))
    v1(0) = 1.0;
  else if ((wz >= wx && wx >= wy) || (wx >= wz && wz >= wy))
    v1(1) = 1.0;
  else
    v1(2) = 1.0;

  const Real projection = v1.dot_product(n);
  v1(0) -= projection * n(0);
  v1(1) -= projection * n(1);
  v1(2) -= projection * n(2);
  const Real v1_norm = v1.norm();
  v1(0) /= v1_norm;
  v1(1) /= v1_norm;
  v1(2) /= v1_norm;

  const Real v0x = v1(1) * n(2) - v1(2) * n(1);
  const Real v0y = v1(2) * n(0) - v1(0) * n(2);
  const Real v0z = v1(0) * n(1) - v1(1) * n(0);

  rot(0, 0) = n(0);
  rot(0, 1) = v1(0);
  rot(0, 2) = -v0x;
  rot(1, 0) = n(1);
  rot(1, 1) = v1(1);
  rot(1, 2) = -v0y;
  rot(2, 0) = n(2);
  rot(2, 1) = v1(2);
  rot(2, 2) = -v0z;
  return rot;
}

/**
 * Computes dR/dF using Chen and Wheeler (1993).
 * Reproduces CohesiveZoneModelTools::computedRdF on device.
 */
KOKKOS_INLINE_FUNCTION void
computedRdF(const Real33 & R, const Real33 & U, Real dR_dF[3][3][3][3])
{
  const Real trace_U = U(0, 0) + U(1, 1) + U(2, 2);
  Real33 Uhat;
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      Uhat(i, j) = (i == j ? trace_U : 0.0) - U(i, j);

  const Real Uhat_det = Uhat.determinant(3);

  for (unsigned int k = 0; k < 3; ++k)
    for (unsigned int l = 0; l < 3; ++l)
      for (unsigned int m = 0; m < 3; ++m)
        for (unsigned int n = 0; n < 3; ++n)
        {
          Real val = 0.0;
          for (unsigned int p = 0; p < 3; ++p)
            for (unsigned int q = 0; q < 3; ++q)
              val +=
                  R(k, p) * (Uhat(p, q) * R(m, q) * Uhat(n, l) - Uhat(p, n) * R(m, q) * Uhat(q, l));

          dR_dF[k][l][m][n] = val / Uhat_det;
        }
}

/**
 * Computes d(F^{-1})/dF from F^{-T}.
 * d(F^{-1})_{ij} / dF_{kl} = - (F^{-1})_{ik} (F^{-1})_{lj} = - FinvT_{ki} * FinvT_{jl}.
 */
KOKKOS_INLINE_FUNCTION void
computedFinversedF(const Real33 & FinvT, Real dFinv[3][3][3][3])
{
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int l = 0; l < 3; ++l)
          dFinv[i][j][k][l] = -FinvT(k, i) * FinvT(j, l);
}

/**
 * Computes derivative of area ratio w.r.t. F.
 * Reproduces CohesiveZoneModelTools::computeDAreaRatioDF on device.
 */
KOKKOS_INLINE_FUNCTION void
computeDAreaRatioDF(const Real33 & FinvT,
                    const Real3 & N,
                    const Real J,
                    const Real dFinv[3][3][3][3],
                    Real dArea_dF[3][3])
{
  const Real3 Fitr_N = FinvT * N;
  const Real Fitr_N_norm = Fitr_N.norm();

  for (unsigned int l = 0; l < 3; ++l)
    for (unsigned int m = 0; m < 3; ++m)
    {
      Real r2temp = 0.0;
      for (unsigned int i = 0; i < 3; ++i)
        for (unsigned int j = 0; j < 3; ++j)
          r2temp += Fitr_N(i) * dFinv[j][i][l][m] * N(j);

      r2temp *= J / Fitr_N_norm;
      dArea_dF[l][m] = J * FinvT(l, m) * Fitr_N_norm + r2temp;
    }
}

} // namespace Moose::Kokkos::CohesiveZoneModelTools
