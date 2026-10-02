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
 * Device-resident interface material whose traction carries a history, so that the acceptance of a
 * step is observable in the solution. The traction of a step is the traction of the previously
 * accepted step plus the elastic increment of the current step, which makes the accumulated state
 * part of the solution and lets a restarted or recovered run be compared against an uninterrupted
 * one.
 *
 * The law is a vehicle for exercising the generic boundary material property lifecycle of an
 * interface material: stateful property declaration, initQpStatefulProperties(), the old state
 * lookup, shift() on an accepted step, and checkpoint store and load. It is not a cohesive law, and
 * the accumulated traction grows with the number of accepted steps.
 */
class KokkosStatefulInterfaceMaterial : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosStatefulInterfaceMaterial(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void initQpStatefulProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                                Datum & elem_datum,
                                                Datum & neighbor_datum,
                                                const unsigned int qp,
                                                const unsigned int neighbor_qp) const;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           const unsigned int qp,
                                           const unsigned int neighbor_qp) const;

private:
  const Real _stiffness;
  const Moose::Kokkos::MaterialProperty<Real> _traction;
  const Moose::Kokkos::MaterialProperty<Real> _traction_old;
  const Moose::Kokkos::MaterialProperty<Real> _dtraction_djump;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosStatefulInterfaceMaterial::initQpStatefulProperties(
    const Moose::Kokkos::InterfaceFaceRecord & /* face */,
    Datum & elem_datum,
    Datum & /* neighbor_datum */,
    const unsigned int qp,
    const unsigned int /* neighbor_qp */) const
{
  // The interface starts untractioned, so the first accepted step adds only its own increment.
  _traction(elem_datum, qp) = 0.;
}

template <typename Derived>
KOKKOS_FUNCTION void
KokkosStatefulInterfaceMaterial::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord & /* face */,
    Datum & elem_datum,
    Datum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  const auto jump =
      _variable_value(elem_datum, qp) - _neighbor_variable_value(neighbor_datum, neighbor_qp);

  // The old state is the traction of the previously accepted step, so the residual stays affine in
  // the solution within a step and the tangent below remains its exact derivative.
  _traction(elem_datum, qp) = _traction_old(elem_datum, qp) + _stiffness * jump;
  _dtraction_djump(elem_datum, qp) = _stiffness;
}