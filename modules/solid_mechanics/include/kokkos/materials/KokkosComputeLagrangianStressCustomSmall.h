//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosMaterial.h"

/**
 * Small-kinematics counterpart of KokkosComputeLagrangianStressCustomPK2 for the Kokkos
 * total-Lagrangian kernel: publishes PK1 = sigma and dPK1/d(grad u) = C : dF_stab/dF_ust from a
 * small stress sigma and its tangent C = dsigma/dstrain (full rank-four tensor with minor
 * symmetries).
 *
 * With stabilize_strain, dF_stab/dF_ust is the additive (small kinematics) F-bar factor
 * I - (1/3) I x I, and the material publishes A = C : I and gamma/3 = 1/3 so that
 * KokkosTotalLagrangianStressDivergence with large_kinematics = false assembles the non-local
 * term (1/3) A tr(delta F_avg). It also publishes the tangent without the F-bar factor, C itself,
 * as dpk1_bypass_fbar for the homogenization kernel.
 */
class KokkosComputeLagrangianStressCustomSmall : public Moose::Kokkos::Material
{
public:
  static InputParameters validParams();

  KokkosComputeLagrangianStressCustomSmall(const InputParameters & parameters);

  void computeProperties() override;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const;

private:
  Moose::Kokkos::MaterialProperty<Real, 2> _small_stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _small_jacobian;
  Moose::Kokkos::MaterialProperty<Real, 2> _pk1_stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _dpk1_d_grad_u;
  bool _need_jacobian;
  Moose::Kokkos::Scalar<bool> _need_jacobian_device;
  const bool _stabilize_strain;
  Moose::Kokkos::MaterialProperty<Real, 2> _A;
  Moose::Kokkos::MaterialProperty<Real, 0> _gamma_over_3;
  Moose::Kokkos::MaterialProperty<Real, 4> _dpk1_bypass_fbar;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosComputeLagrangianStressCustomSmall::computeQpProperties(const unsigned int qp,
                                                              Datum & datum) const
{
  const auto sigma = _small_stress(datum, qp);
  auto pk1 = _pk1_stress(datum, qp);
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      pk1(i, j) = sigma(i, j);

  if (!_need_jacobian_device)
    return;

  const auto C = _small_jacobian(datum, qp);
  auto dpk1 = _dpk1_d_grad_u(datum, qp);
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int l = 0; l < 3; ++l)
          dpk1(i, j, k, l) = C(i, j, k, l);

  if (!_stabilize_strain)
    return;

  auto bypass = _dpk1_bypass_fbar(datum, qp);
  auto A = _A(datum, qp);
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
    {
      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int l = 0; l < 3; ++l)
          bypass(i, j, k, l) = C(i, j, k, l);

      A(i, j) = C(i, j, 0, 0) + C(i, j, 1, 1) + C(i, j, 2, 2);
      for (unsigned int k = 0; k < 3; ++k)
        dpk1(i, j, k, k) -= A(i, j) / 3;
    }
  _gamma_over_3(datum, qp) = 1.0 / 3;
}
