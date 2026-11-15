//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosShiftedInterfaceKernel.h"

/**
 * Test-only shifted interface kernel applying a linear penalty traction on the shifted displacement
 * jump. The host shifted formulation in this module has no counterpart of this class, because its
 * production path assembles the cohesive zone model rather than a penalty law. This kernel exists
 * so that the generic shifted assembly machinery can be verified on its own, namely the shortest
 * distance to the surface, the distance-based shifted value and shape helpers, the signed area
 * correction, and all four same-component Jacobian blocks, against a law whose tangent is known in
 * closed form and therefore needs no material to supply it.
 */
class KokkosShiftedPenaltyInterface : public Moose::Kokkos::ShiftedInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosShiftedPenaltyInterface(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpResidual(Moose::DGResidualType type,
                                         unsigned int i,
                                         unsigned int qp,
                                         unsigned int neighbor_qp,
                                         AssemblyDatum & elem_datum,
                                         AssemblyDatum & neighbor_datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpJacobian(Moose::DGJacobianType type,
                                         unsigned int i,
                                         unsigned int j,
                                         unsigned int qp,
                                         unsigned int neighbor_qp,
                                         AssemblyDatum & elem_datum,
                                         AssemblyDatum & neighbor_datum) const;

private:
  const Real _penalty;
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosShiftedPenaltyInterface::computeQpResidual(const Moose::DGResidualType type,
                                                 const unsigned int i,
                                                 const unsigned int qp,
                                                 const unsigned int neighbor_qp,
                                                 AssemblyDatum & elem_datum,
                                                 AssemblyDatum & neighbor_datum) const
{
  const auto distance = surrogateDistanceVector(elem_datum, qp);
  const auto jump =
      shiftedValue(_u, _grad_u, elem_datum, distance, qp) -
      shiftedValue(_u_neighbor, _grad_u_neighbor, neighbor_datum, distance, neighbor_qp);

  return (type == Moose::Element ? _test(elem_datum, i, qp)
                                 : -_test_neighbor(neighbor_datum, i, neighbor_qp)) *
         areaCorrection(elem_datum, qp) * _penalty * jump;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosShiftedPenaltyInterface::computeQpJacobian(const Moose::DGJacobianType type,
                                                 const unsigned int i,
                                                 const unsigned int j,
                                                 const unsigned int qp,
                                                 const unsigned int neighbor_qp,
                                                 AssemblyDatum & elem_datum,
                                                 AssemblyDatum & neighbor_datum) const
{
  const auto distance = surrogateDistanceVector(elem_datum, qp);
  const auto elem_test = _test(elem_datum, i, qp);
  const auto neighbor_test = _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto coefficient = areaCorrection(elem_datum, qp) * _penalty;

  switch (type)
  {
    case Moose::ElementElement:
      return elem_test * coefficient * shiftedPhi(_phi, _grad_phi, elem_datum, distance, j, qp);
    case Moose::ElementNeighbor:
      return -elem_test * coefficient *
             shiftedPhi(
                 _phi_neighbor, _grad_phi_neighbor, neighbor_datum, distance, j, neighbor_qp);
    case Moose::NeighborElement:
      return -neighbor_test * coefficient *
             shiftedPhi(_phi, _grad_phi, elem_datum, distance, j, qp);
    case Moose::NeighborNeighbor:
      return neighbor_test * coefficient *
             shiftedPhi(
                 _phi_neighbor, _grad_phi_neighbor, neighbor_datum, distance, j, neighbor_qp);
  }

  return 0;
}
