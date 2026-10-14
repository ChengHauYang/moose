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
 * Write the primary-side entry of the two-sided Kokkos property addressing fixture.
 *
 * KokkosTwoSidedAddressNeighborMaterial writes the matching neighbor-side entry into the neighbor
 * property storage, and KokkosTwoSidedAddressInterfaceDiffusion reads both back into the two
 * residual rows of each oriented face. Every identifying part of the address therefore lands
 * directly in the solution, so a swapped storage, a wrong face offset, or a wrong quadrature point
 * pairing changes the solution rather than cancelling between the two rows.
 *
 * The value written at one quadrature point is
 *
 *   side_tag + 0.5 * face_tag + 0.25 * qp + 0.5 * x + weight * variable
 *
 * with side_tag distinguishing the physical side, face_tag the contiguous element ID of the
 * primary element of the oriented face, qp the primary-side quadrature point index, x the abscissa
 * of the quadrature point, and variable the primary-side coupled value. The weights are chosen so
 * that no two parts can be confused, and so that the whole address stays O(10) on the meshes this
 * fixture uses: an address of order 1e4 would leave the residual dominated by a part that carries
 * no derivative, and the finite-difference Jacobian check would then measure its own roundoff
 * rather than the assembled Jacobian.
 */
class KokkosTwoSidedAddressMaterial : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosTwoSidedAddressMaterial(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           const unsigned int qp,
                                           const unsigned int neighbor_qp) const;

  /// Side tag of the primary-side entries, see KokkosTwoSidedAddressNeighborMaterial.
  static constexpr Real side_tag = 0.125;

private:
  const Real _weight;
  const Moose::Kokkos::VariableValue _variable_value;
  Moose::Kokkos::MaterialProperty<Real> _side_address;
  Moose::Kokkos::MaterialProperty<Real> _dside_address_djump;
};

/**
 * Write the neighbor-side entry of the same fixture into the neighbor property storage.
 *
 * Setting _material_data_type to NEIGHBOR_MATERIAL_DATA is what makes this class declare into the
 * neighbor storage rather than the boundary storage, so the two sides are two distinct arrays
 * holding the same kind of property. That is the contract getKokkosMaterialPropertyByName and
 * getKokkosNeighborMaterialPropertyByName rely on, and the reason a consumer can hold two values
 * for one physical interface face without any SCZM-specific naming.
 *
 * The value is written at (neighbor_datum, neighbor_qp), which is the neighbor element's own face
 * quadrature point address rather than the primary element's. It carries the primary-side face tag
 * and quadrature point index so that the two rows of a face describe the same physical quadrature
 * point, while the abscissa and the variable are taken from the neighbor side.
 */
class KokkosTwoSidedAddressNeighborMaterial : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosTwoSidedAddressNeighborMaterial(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           const unsigned int qp,
                                           const unsigned int neighbor_qp) const;

  /// Side tag of the neighbor-side entries, see KokkosTwoSidedAddressMaterial.
  static constexpr Real side_tag = 0.625;

private:
  /**
   * Require that a parallel run actually writes at least one interface face whose neighbor element
   * is a one-layer ghost.
   *
   * The neighbor-side entry is written through the neighbor element's own face quadrature point
   * address, which resolves to Kokkos element data only when that neighbor is locally owned or a
   * one-layer ghost. A repartitioning that kept every interface face together with its neighbor
   * would leave that path uncovered while still passing the serial test, so the coverage this test
   * exists to provide is asserted rather than assumed.
   */
  void checkGhostNeighborCoverage();

  const Real _weight;
  const Moose::Kokkos::VariableValue _neighbor_variable_value;
  Moose::Kokkos::MaterialProperty<Real> _neighbor_address;
  Moose::Kokkos::MaterialProperty<Real> _dneighbor_address_djump;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosTwoSidedAddressMaterial::computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                                   Datum & elem_datum,
                                                   Datum & /*neighbor_datum*/,
                                                   const unsigned int qp,
                                                   const unsigned int /*neighbor_qp*/) const
{
  _side_address(elem_datum, qp) = side_tag + 0.5 * face.elem + 0.25 * qp +
                                  0.5 * elem_datum.q_point(qp)(0) +
                                  _weight * _variable_value(elem_datum, qp);
  _dside_address_djump(elem_datum, qp) = _weight;
}

template <typename Derived>
KOKKOS_FUNCTION void
KokkosTwoSidedAddressNeighborMaterial::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord & face,
    Datum & /*elem_datum*/,
    Datum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  _neighbor_address(neighbor_datum, neighbor_qp) =
      side_tag + 0.5 * face.elem + 0.25 * qp + 0.5 * neighbor_datum.q_point(neighbor_qp)(0) +
      _weight * _neighbor_variable_value(neighbor_datum, neighbor_qp);
  _dneighbor_address_djump(neighbor_datum, neighbor_qp) = _weight;
}
