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
 * Test-only device oracle for symmetric NEML2 Kokkos interface retrievers.
 *
 * The primary and neighbor specializations deliberately use the corresponding property getter, so
 * each specialization verifies the storage selected by interface_output_side. The checked shear
 * stress and shear tangent exercise the Mandel-to-tensor conversion in addition to interface QP
 * addressing.
 */
template <bool neighbor>
class KokkosNEML2InterfaceRetrieverOracle : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosNEML2InterfaceRetrieverOracle(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           const unsigned int qp,
                                           const unsigned int neighbor_qp) const;

private:
  const Real _expected_normal_stress;
  const Real _expected_transverse_stress;
  const Real _expected_shear_stress;
  const Real _expected_shear_tangent;
  const Moose::Kokkos::MaterialProperty<Real, 2> _stress;
  const Moose::Kokkos::MaterialProperty<Real, 4> _tangent;
};

using KokkosNEML2PrimaryInterfaceRetrieverOracle = KokkosNEML2InterfaceRetrieverOracle<false>;
using KokkosNEML2NeighborInterfaceRetrieverOracle = KokkosNEML2InterfaceRetrieverOracle<true>;

template <bool neighbor>
template <typename Derived>
KOKKOS_FUNCTION void
KokkosNEML2InterfaceRetrieverOracle<neighbor>::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord &,
    Datum & elem_datum,
    Datum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  auto & datum = neighbor ? neighbor_datum : elem_datum;
  const auto storage_qp = neighbor ? neighbor_qp : qp;
  const auto stress = _stress(datum, storage_qp);
  const auto tangent = _tangent(datum, storage_qp);
  // The controlled linear-elastic values are exactly representable, so 1e-24 permits a 1e-12
  // component error while detecting any incorrect side, QP, or Mandel-component mapping.
  constexpr Real tolerance_squared = 1e-24;
  const auto normal_error = stress(0, 0) - _expected_normal_stress;
  const auto transverse_error = stress(1, 1) - _expected_transverse_stress;
  const auto shear_error = stress(0, 1) - _expected_shear_stress;
  const auto tangent_error = tangent(0, 1, 0, 1) - _expected_shear_tangent;
  if (normal_error * normal_error > tolerance_squared ||
      transverse_error * transverse_error > tolerance_squared ||
      shear_error * shear_error > tolerance_squared ||
      tangent_error * tangent_error > tolerance_squared)
    ::Kokkos::abort("NEML2 Kokkos interface Retriever returned an unexpected property value.");
}
