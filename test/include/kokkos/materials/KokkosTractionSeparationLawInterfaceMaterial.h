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
 * Device-resident interface material implementing a traction-separation law whose traction carries
 * a history, so that the acceptance of a step is observable in the solution. The traction of a step
 * is the traction of the previously accepted step plus the elastic increment of the current step,
 * which makes the accumulated state part of the solution and lets a restarted or recovered run be
 * compared against an uninterrupted one.
 *
 * The law is a vehicle for exercising the generic boundary material property lifecycle of an
 * interface material: stateful property declaration, initQpStatefulProperties(), old and older
 * state lookup, shift() on an accepted step, rejected-step restoration, and checkpoint store and
 * load. It is not a cohesive law, and the accumulated traction grows with the number of accepted
 * steps.
 */
class KokkosTractionSeparationLawInterfaceMaterial : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosTractionSeparationLawInterfaceMaterial(const InputParameters & parameters);

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
  const Real _state_increment;
  const Moose::Kokkos::MaterialProperty<Real> _traction;
  const Moose::Kokkos::MaterialProperty<Real> _traction_old;
  const Moose::Kokkos::MaterialProperty<Real> _traction_older;
  const Moose::Kokkos::MaterialProperty<Real> _traction_at_old_state;
  const Moose::Kokkos::MaterialProperty<Real> _traction_at_older_state;
  const Moose::Kokkos::MaterialProperty<Real> _zero_tangent;
  const Moose::Kokkos::MaterialProperty<Real> _dtraction_djump;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosTractionSeparationLawInterfaceMaterial::initQpStatefulProperties(
    const Moose::Kokkos::InterfaceFaceRecord & /* face */,
    Datum & elem_datum,
    Datum & /* neighbor_datum */,
    const unsigned int qp,
    const unsigned int /* neighbor_qp */) const
{
  // The interface starts untractioned, so the first accepted step adds only its own increment.
  _traction(elem_datum, qp) = 0.;
  _traction_at_old_state(elem_datum, qp) = 0.;
  _traction_at_older_state(elem_datum, qp) = 0.;
  _zero_tangent(elem_datum, qp) = 0.;
}

template <typename Derived>
KOKKOS_FUNCTION void
KokkosTractionSeparationLawInterfaceMaterial::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord & /* face */,
    Datum & elem_datum,
    Datum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  const auto jump =
      _variable_value(elem_datum, qp) - _neighbor_variable_value(neighbor_datum, neighbor_qp);

  // Old and older are the two preceding accepted states. Copying them into separate current
  // properties lets interface kernels observe both without changing the history law, because an
  // interface kernel can only read the current state of a property.
  _traction_at_old_state(elem_datum, qp) = _traction_old(elem_datum, qp);
  _traction_at_older_state(elem_datum, qp) = _traction_older(elem_datum, qp);
  _zero_tangent(elem_datum, qp) = 0.;

  _traction(elem_datum, qp) = _traction_old(elem_datum, qp) + _state_increment + _stiffness * jump;
  _dtraction_djump(elem_datum, qp) = _stiffness;
}
