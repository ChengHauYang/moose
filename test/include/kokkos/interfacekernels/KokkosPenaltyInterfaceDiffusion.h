//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosInterfaceKernel.h"

class KokkosPenaltyInterfaceDiffusion : public Moose::Kokkos::InterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosPenaltyInterfaceDiffusion(const InputParameters & parameters);

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

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpOffDiagJacobian(Moose::DGJacobianType type,
                                                unsigned int i,
                                                unsigned int j,
                                                unsigned int jvar,
                                                unsigned int qp,
                                                unsigned int neighbor_qp,
                                                AssemblyDatum & elem_datum,
                                                AssemblyDatum & neighbor_datum) const;

private:
  const Real _penalty;
  const Moose::Kokkos::VariableValue _primary_coupled_value;
  const Moose::Kokkos::VariableValue _neighbor_coupled_value;
  const unsigned int _primary_coupled_var;
  const unsigned int _neighbor_coupled_var;
  const Moose::Kokkos::MaterialProperty<Real> _primary_coefficient;
  const Moose::Kokkos::MaterialProperty<Real> _neighbor_coefficient;
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosPenaltyInterfaceDiffusion::computeQpResidual(const Moose::DGResidualType type,
                                                   const unsigned int i,
                                                   const unsigned int qp,
                                                   const unsigned int neighbor_qp,
                                                   AssemblyDatum & elem_datum,
                                                   AssemblyDatum & neighbor_datum) const
{
  const auto penalty = _penalty * _primary_coefficient(elem_datum, qp) *
                       _neighbor_coefficient(neighbor_datum, neighbor_qp);
  const auto jump = _u(elem_datum, qp) - _u_neighbor(neighbor_datum, neighbor_qp) +
                    _primary_coupled_value(elem_datum, qp) -
                    _neighbor_coupled_value(neighbor_datum, neighbor_qp);

  switch (type)
  {
    case Moose::Element:
      return _test(elem_datum, i, qp) * penalty * jump;
    case Moose::Neighbor:
      return -_test_neighbor(neighbor_datum, i, neighbor_qp) * penalty * jump;
  }

  return 0;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosPenaltyInterfaceDiffusion::computeQpJacobian(const Moose::DGJacobianType type,
                                                   const unsigned int i,
                                                   const unsigned int j,
                                                   const unsigned int qp,
                                                   const unsigned int neighbor_qp,
                                                   AssemblyDatum & elem_datum,
                                                   AssemblyDatum & neighbor_datum) const
{
  const auto penalty = _penalty * _primary_coefficient(elem_datum, qp) *
                       _neighbor_coefficient(neighbor_datum, neighbor_qp);

  switch (type)
  {
    case Moose::ElementElement:
      return _test(elem_datum, i, qp) * penalty * _phi(elem_datum, j, qp);
    case Moose::ElementNeighbor:
      return -_test(elem_datum, i, qp) * penalty *
             _phi_neighbor(neighbor_datum, j, neighbor_qp);
    case Moose::NeighborElement:
      return -_test_neighbor(neighbor_datum, i, neighbor_qp) * penalty *
             _phi(elem_datum, j, qp);
    case Moose::NeighborNeighbor:
      return _test_neighbor(neighbor_datum, i, neighbor_qp) * penalty *
             _phi_neighbor(neighbor_datum, j, neighbor_qp);
  }

  return 0;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosPenaltyInterfaceDiffusion::computeQpOffDiagJacobian(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int jvar,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum) const
{
  const auto penalty = _penalty * _primary_coefficient(elem_datum, qp) *
                       _neighbor_coefficient(neighbor_datum, neighbor_qp);

  if (jvar == _primary_coupled_var)
    switch (type)
    {
      case Moose::ElementElement:
        return _test(elem_datum, i, qp) * penalty * _phi(elem_datum, j, qp);
      case Moose::NeighborElement:
        return -_test_neighbor(neighbor_datum, i, neighbor_qp) * penalty *
               _phi(elem_datum, j, qp);
      case Moose::ElementNeighbor:
      case Moose::NeighborNeighbor:
        return 0;
    }

  if (jvar == _neighbor_coupled_var)
    switch (type)
    {
      case Moose::ElementNeighbor:
        return -_test(elem_datum, i, qp) * penalty *
               _phi_neighbor(neighbor_datum, j, neighbor_qp);
      case Moose::NeighborNeighbor:
        return _test_neighbor(neighbor_datum, i, neighbor_qp) * penalty *
               _phi_neighbor(neighbor_datum, j, neighbor_qp);
      case Moose::ElementElement:
      case Moose::NeighborElement:
        return 0;
    }

  return 0;
}
