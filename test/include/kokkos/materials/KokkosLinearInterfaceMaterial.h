//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosInterfaceMaterial.h"

/**
 * Provides a device-resident linear traction law driven by the two-sided jump of a scalar interface
 * variable. The traction is written once, at the property index of the primary side of each
 * interface face, and the interface kernel reading it assembles both of its residual rows from that
 * single value.
 */
class KokkosLinearInterfaceMaterial : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosLinearInterfaceMaterial(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           const unsigned int qp,
                                           const unsigned int neighbor_qp) const;

private:
  /**
   * Assert that a distributed run of this test really evaluates an interface face whose neighbor
   * element is a one-layer ghost. The neighbor-side interface datum has to read that ghost element
   * from the Kokkos mesh instead of the side topology table, which is only filled for active local
   * elements, so this test is only meaningful while the interface faces are split across ranks.
   */
  void checkGhostNeighborCoverage();

  const Real _stiffness;
  Moose::Kokkos::MaterialProperty<Real> _traction;
  Moose::Kokkos::MaterialProperty<Real> _dtraction_djump;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosLinearInterfaceMaterial::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord & /* face */,
    Datum & elem_datum,
    Datum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  const auto jump =
      _variable_value(elem_datum, qp) - _neighbor_variable_value(neighbor_datum, neighbor_qp);

  _traction(elem_datum, qp) = _stiffness * jump;
  _dtraction_djump(elem_datum, qp) = _stiffness;
}
