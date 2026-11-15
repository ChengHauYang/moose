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

/**
 * Test-only device oracle for the interface PK2-to-PK1 conversion.
 *
 * The primary and neighbor specializations independently read their corresponding interface
 * property storage and verify every component of P = F S and dP/dF.
 */
template <bool neighbor>
class KokkosPK2ToPK1InterfaceOracle : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosPK2ToPK1InterfaceOracle(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           unsigned int qp,
                                           unsigned int neighbor_qp) const;

private:
  const Moose::Kokkos::VariableGradient _grad_displacements;
  const Moose::Kokkos::MaterialProperty<Real, 2> _pk2_stress;
  const Moose::Kokkos::MaterialProperty<Real, 4> _dpk2_dF;
  const Moose::Kokkos::MaterialProperty<Real, 2> _pk1_stress;
  const Moose::Kokkos::MaterialProperty<Real, 4> _dpk1_dF;
};

using KokkosPK2ToPK1PrimaryInterfaceOracle = KokkosPK2ToPK1InterfaceOracle<false>;
using KokkosPK2ToPK1NeighborInterfaceOracle = KokkosPK2ToPK1InterfaceOracle<true>;

template <bool neighbor>
template <typename Derived>
KOKKOS_FUNCTION void
KokkosPK2ToPK1InterfaceOracle<neighbor>::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord &,
    Datum & elem_datum,
    Datum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  auto & datum = neighbor ? neighbor_datum : elem_datum;
  const auto storage_qp = neighbor ? neighbor_qp : qp;
  const auto pk2 = _pk2_stress(datum, storage_qp);
  const auto dpk2_dF = _dpk2_dF(datum, storage_qp);
  const auto pk1 = _pk1_stress(datum, storage_qp);
  const auto dpk1_dF = _dpk1_dF(datum, storage_qp);

  Real F[3][3];
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int J = 0; J < 3; ++J)
      F[i][J] = (i == J) + _grad_displacements(datum, storage_qp, i)(J);

  constexpr Real tolerance_squared = 1e-24;
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int J = 0; J < 3; ++J)
    {
      Real expected_pk1 = 0;
      for (unsigned int K = 0; K < 3; ++K)
        expected_pk1 += F[i][K] * pk2(K, J);
      const auto pk1_error = pk1(i, J) - expected_pk1;
      if (pk1_error * pk1_error > tolerance_squared)
        ::Kokkos::abort("PK2-to-PK1 interface conversion returned an unexpected PK1 component.");

      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int L = 0; L < 3; ++L)
        {
          Real expected_tangent = (i == k) * pk2(L, J);
          for (unsigned int K = 0; K < 3; ++K)
            expected_tangent += F[i][K] * dpk2_dF(K, J, k, L);
          const auto tangent_error = dpk1_dF(i, J, k, L) - expected_tangent;
          if (tangent_error * tangent_error > tolerance_squared)
            ::Kokkos::abort(
                "PK2-to-PK1 interface conversion returned an unexpected tangent component.");
        }
    }
}
