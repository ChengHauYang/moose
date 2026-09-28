//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosInterfaceKernel.h"

/**
 * Assembles a traction supplied by a Kokkos interface material. The traction is read at the primary
 * side of each interface face and applied with opposite signs to the primary and neighbor residual
 * rows, which verifies that a property written once on the primary element owner is consistently
 * visible from both sides.
 */
class KokkosLinearInterfaceDiffusion : public Moose::Kokkos::InterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosLinearInterfaceDiffusion(const InputParameters & parameters);

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
  const Moose::Kokkos::MaterialProperty<Real> _traction;
  const Moose::Kokkos::MaterialProperty<Real> _dtraction_djump;
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosLinearInterfaceDiffusion::computeQpResidual(const Moose::DGResidualType type,
                                                  const unsigned int i,
                                                  const unsigned int qp,
                                                  const unsigned int neighbor_qp,
                                                  AssemblyDatum & elem_datum,
                                                  AssemblyDatum & neighbor_datum) const
{
  const auto traction = _traction(elem_datum, qp);

  switch (type)
  {
    case Moose::Element:
      return _test(elem_datum, i, qp) * traction;
    case Moose::Neighbor:
      return -_test_neighbor(neighbor_datum, i, neighbor_qp) * traction;
  }

  return 0;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosLinearInterfaceDiffusion::computeQpJacobian(const Moose::DGJacobianType type,
                                                  const unsigned int i,
                                                  const unsigned int j,
                                                  const unsigned int qp,
                                                  const unsigned int neighbor_qp,
                                                  AssemblyDatum & elem_datum,
                                                  AssemblyDatum & neighbor_datum) const
{
  const auto dtraction_djump = _dtraction_djump(elem_datum, qp);

  switch (type)
  {
    case Moose::ElementElement:
      return _test(elem_datum, i, qp) * dtraction_djump * _phi(elem_datum, j, qp);
    case Moose::ElementNeighbor:
      return -_test(elem_datum, i, qp) * dtraction_djump *
             _phi_neighbor(neighbor_datum, j, neighbor_qp);
    case Moose::NeighborElement:
      return -_test_neighbor(neighbor_datum, i, neighbor_qp) * dtraction_djump *
             _phi(elem_datum, j, qp);
    case Moose::NeighborNeighbor:
      return _test_neighbor(neighbor_datum, i, neighbor_qp) * dtraction_djump *
             _phi_neighbor(neighbor_datum, j, neighbor_qp);
  }

  return 0;
}
