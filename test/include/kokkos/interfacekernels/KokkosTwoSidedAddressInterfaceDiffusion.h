//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceKernel.h"

/**
 * Assembles the primary-side row of an interface face from the boundary property storage and the
 * neighbor-side row from the neighbor property storage.
 *
 * This kernel consumes the fixture written by KokkosTwoSidedAddressMaterial and
 * KokkosTwoSidedAddressNeighborMaterial. The two rows read the two halves of one physical face
 * through the standard property getters, each at the quadrature point of its own side, so the
 * assembled solution records what each side saw. Because every identifying part of the address
 * reaches the solution without cancelling, reading the wrong face, the wrong quadrature point, or
 * the wrong storage changes the solution.
 */
class KokkosTwoSidedAddressInterfaceDiffusion : public Moose::Kokkos::InterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosTwoSidedAddressInterfaceDiffusion(const InputParameters & parameters);

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
  /// Primary-side address, from the boundary property storage.
  const Moose::Kokkos::MaterialProperty<Real> _side_address;
  /// Neighbor-side address, from the neighbor property storage.
  const Moose::Kokkos::MaterialProperty<Real> _neighbor_address;
  /// Derivative of the primary-side address with respect to the primary-side jump.
  const Moose::Kokkos::MaterialProperty<Real> _dside_address_djump;
  /// Derivative of the neighbor-side address with respect to the neighbor-side jump.
  const Moose::Kokkos::MaterialProperty<Real> _dneighbor_address_djump;
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosTwoSidedAddressInterfaceDiffusion::computeQpResidual(const Moose::DGResidualType type,
                                                           const unsigned int i,
                                                           const unsigned int qp,
                                                           const unsigned int neighbor_qp,
                                                           AssemblyDatum & elem_datum,
                                                           AssemblyDatum & neighbor_datum) const
{
  if (type == Moose::Element)
    return _test(elem_datum, i, qp) * _side_address(elem_datum, qp);

  return -_test_neighbor(neighbor_datum, i, neighbor_qp) *
         _neighbor_address(neighbor_datum, neighbor_qp);
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosTwoSidedAddressInterfaceDiffusion::computeQpJacobian(const Moose::DGJacobianType type,
                                                           const unsigned int i,
                                                           const unsigned int j,
                                                           const unsigned int qp,
                                                           const unsigned int neighbor_qp,
                                                           AssemblyDatum & elem_datum,
                                                           AssemblyDatum & neighbor_datum) const
{
  // Only the variable-dependent part of each address contributes, and the place-valued part is a
  // constant of the face. Each row depends on its own side's variable alone, so the cross blocks
  // vanish. The neighbor row carries the same minus sign as its residual.
  if (type == Moose::ElementElement)
    return _test(elem_datum, i, qp) * _dside_address_djump(elem_datum, qp) *
           _phi(elem_datum, j, qp);

  if (type == Moose::NeighborNeighbor)
    return -_test_neighbor(neighbor_datum, i, neighbor_qp) *
           _dneighbor_address_djump(neighbor_datum, neighbor_qp) *
           _phi_neighbor(neighbor_datum, j, neighbor_qp);

  return 0.0;
}
