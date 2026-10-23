//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceMaterial.h"
#include "KokkosMaterial.h"

#include <type_traits>

/**
 * Converts full PK2 stress and its deformation-gradient derivative to PK1 quantities.
 *
 * The bulk specialization also supports the total-mode F-bar Jacobian chain. Interface
 * specializations operate on the primary or neighbor material-property storage and intentionally
 * reject F-bar because the SCZM kernel does not assemble its non-local coupling.
 */
template <bool interface, bool neighbor = false>
class KokkosComputeLagrangianStressCustomPK2Templ
  : public std::conditional_t<interface, Moose::Kokkos::InterfaceMaterial, Moose::Kokkos::Material>
{
  static_assert(interface || !neighbor);

public:
  using Base =
      std::conditional_t<interface, Moose::Kokkos::InterfaceMaterial, Moose::Kokkos::Material>;

  static InputParameters validParams();

  KokkosComputeLagrangianStressCustomPK2Templ(const InputParameters & parameters);

  void computeProperties() override;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           const unsigned int qp,
                                           const unsigned int neighbor_qp) const;

private:
  template <typename Derived>
  KOKKOS_FUNCTION void convertPK2ToPK1(Datum & datum, const unsigned int qp) const;

  const unsigned int _ndisp;
  const Moose::Kokkos::VariableGradient _grad_displacements;
  Moose::Kokkos::MaterialProperty<Real, 2> _pk2_stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _dpk2_dF;
  Moose::Kokkos::MaterialProperty<Real, 2> _pk1_stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _dpk1_d_grad_u;
  bool _need_jacobian;
  Moose::Kokkos::Scalar<bool> _need_jacobian_device;

  bool _stabilize_strain;
  Moose::Kokkos::Scalar<bool> _stabilize_strain_device;
  Moose::Kokkos::MaterialProperty<Real, 2> _F_avg;
  Moose::Kokkos::MaterialProperty<Real, 2> _F_avg_invT;
  Moose::Kokkos::MaterialProperty<Real, 2> _A;
  Moose::Kokkos::MaterialProperty<Real, 0> _gamma_over_3;
};

using KokkosComputeLagrangianStressCustomPK2 =
    KokkosComputeLagrangianStressCustomPK2Templ<false, false>;
using KokkosInterfaceComputeLagrangianStressCustomPK2 =
    KokkosComputeLagrangianStressCustomPK2Templ<true, false>;
using KokkosNeighborComputeLagrangianStressCustomPK2 =
    KokkosComputeLagrangianStressCustomPK2Templ<true, true>;

template <bool interface, bool neighbor>
template <typename Derived>
KOKKOS_FUNCTION void
KokkosComputeLagrangianStressCustomPK2Templ<interface, neighbor>::computeQpProperties(
    const unsigned int qp, Datum & datum) const
{
  convertPK2ToPK1<Derived>(datum, qp);
}

template <bool interface, bool neighbor>
template <typename Derived>
KOKKOS_FUNCTION void
KokkosComputeLagrangianStressCustomPK2Templ<interface, neighbor>::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord &,
    Datum & elem_datum,
    Datum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  if constexpr (neighbor)
    convertPK2ToPK1<Derived>(neighbor_datum, neighbor_qp);
  else
    convertPK2ToPK1<Derived>(elem_datum, qp);
}

template <bool interface, bool neighbor>
template <typename Derived>
KOKKOS_FUNCTION void
KokkosComputeLagrangianStressCustomPK2Templ<interface, neighbor>::convertPK2ToPK1(
    Datum & datum, const unsigned int qp) const
{
  const auto pk2 = _pk2_stress(datum, qp);
  const auto dpk2_dF = _dpk2_dF(datum, qp);
  auto pk1 = _pk1_stress(datum, qp);
  auto dpk1_d_grad_u = _dpk1_d_grad_u(datum, qp);

  Real F[3][3];
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      F[i][j] = (i == j) + _grad_displacements(datum, qp, i)(j);

  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int J = 0; J < 3; ++J)
    {
      pk1(i, J) = 0;
      for (unsigned int K = 0; K < 3; ++K)
        pk1(i, J) += F[i][K] * pk2(K, J);
    }

  if (!_need_jacobian_device)
    return;

  if (!_stabilize_strain_device)
  {
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int J = 0; J < 3; ++J)
        for (unsigned int k = 0; k < 3; ++k)
          for (unsigned int L = 0; L < 3; ++L)
          {
            dpk1_d_grad_u(i, J, k, L) = (i == k) * pk2(L, J);
            for (unsigned int K = 0; K < 3; ++K)
              dpk1_d_grad_u(i, J, k, L) += F[i][K] * dpk2_dF(K, J, k, L);
          }
    return;
  }

  const Real detF = F[0][0] * (F[1][1] * F[2][2] - F[1][2] * F[2][1]) -
                    F[0][1] * (F[1][0] * F[2][2] - F[1][2] * F[2][0]) +
                    F[0][2] * (F[1][0] * F[2][1] - F[1][1] * F[2][0]);
  const auto F_avg = _F_avg(datum, qp);
  const Real detFavg = F_avg(0, 0) * (F_avg(1, 1) * F_avg(2, 2) - F_avg(1, 2) * F_avg(2, 1)) -
                       F_avg(0, 1) * (F_avg(1, 0) * F_avg(2, 2) - F_avg(1, 2) * F_avg(2, 0)) +
                       F_avg(0, 2) * (F_avg(1, 0) * F_avg(2, 1) - F_avg(1, 1) * F_avg(2, 0));
  const Real ratio = detFavg / detF;
  const Real gamma = (ratio < 0.0 ? -1.0 : 1.0) * ::Kokkos::pow(::Kokkos::fabs(ratio), 1.0 / 3.0);

  Real Cof[3][3];
  Cof[0][0] = F[1][1] * F[2][2] - F[1][2] * F[2][1];
  Cof[0][1] = -(F[1][0] * F[2][2] - F[1][2] * F[2][0]);
  Cof[0][2] = F[1][0] * F[2][1] - F[1][1] * F[2][0];
  Cof[1][0] = -(F[0][1] * F[2][2] - F[0][2] * F[2][1]);
  Cof[1][1] = F[0][0] * F[2][2] - F[0][2] * F[2][0];
  Cof[1][2] = -(F[0][0] * F[2][1] - F[0][1] * F[2][0]);
  Cof[2][0] = F[0][1] * F[1][2] - F[0][2] * F[1][1];
  Cof[2][1] = -(F[0][0] * F[1][2] - F[0][2] * F[1][0]);
  Cof[2][2] = F[0][0] * F[1][1] - F[0][1] * F[1][0];
  Real Fust_invT[3][3];
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      Fust_invT[i][j] = Cof[i][j] / detF;

  auto A_prop = _A(datum, qp);
  Real Aij[3][3];
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int J = 0; J < 3; ++J)
    {
      Real s = 0.0;
      for (unsigned int K = 0; K < 3; ++K)
        for (unsigned int m = 0; m < 3; ++m)
          for (unsigned int n = 0; n < 3; ++n)
            s += F[i][K] * dpk2_dF(K, J, m, n) * F[m][n];
      Aij[i][J] = s;
      A_prop(i, J) = s;
    }

  const Real g3 = gamma / 3.0;
  _gamma_over_3(datum, qp) = g3;
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int J = 0; J < 3; ++J)
      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int L = 0; L < 3; ++L)
        {
          Real v = (i == k) * pk2(L, J);
          for (unsigned int K = 0; K < 3; ++K)
            v += gamma * F[i][K] * dpk2_dF(K, J, k, L);
          dpk1_d_grad_u(i, J, k, L) = v - g3 * Aij[i][J] * Fust_invT[k][L];
        }
}
