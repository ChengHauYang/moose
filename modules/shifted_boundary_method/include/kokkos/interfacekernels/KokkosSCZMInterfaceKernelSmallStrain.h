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
 * Kokkos shifted cohesive-zone interface kernel for small-strain displacement variables.
 */
class KokkosSCZMInterfaceKernelSmallStrain : public Moose::Kokkos::ShiftedInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosSCZMInterfaceKernelSmallStrain(const InputParameters & parameters);

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

protected:
  KOKKOS_FUNCTION Real computeDResidualDDisplacement(Moose::DGJacobianType type,
                                                     unsigned int i,
                                                     unsigned int j,
                                                     unsigned int component_j,
                                                     unsigned int qp,
                                                     unsigned int neighbor_qp,
                                                     AssemblyDatum & elem_datum,
                                                     AssemblyDatum & neighbor_datum) const;

  const unsigned int _component;
  const unsigned int _ndisp;
  Moose::Kokkos::Array<unsigned int> _disp_vars;
  const Moose::Kokkos::MaterialProperty<Real, 1> _traction_global;
  const Moose::Kokkos::MaterialProperty<Real, 2> _dtraction_djump_global;
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeQpResidual(const Moose::DGResidualType type,
                                                        const unsigned int i,
                                                        const unsigned int qp,
                                                        const unsigned int neighbor_qp,
                                                        AssemblyDatum & elem_datum,
                                                        AssemblyDatum & neighbor_datum) const
{
  const auto test = type == Moose::Element ? _test(elem_datum, i, qp)
                                           : _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto sign = type == Moose::Element ? -1.0 : 1.0;

  return sign * test * areaCorrection(elem_datum, qp) *
         _traction_global(elem_datum, qp)(_component);
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeQpJacobian(const Moose::DGJacobianType type,
                                                        const unsigned int i,
                                                        const unsigned int j,
                                                        const unsigned int qp,
                                                        const unsigned int neighbor_qp,
                                                        AssemblyDatum & elem_datum,
                                                        AssemblyDatum & neighbor_datum) const
{
  return computeDResidualDDisplacement(
      type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum);
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeQpOffDiagJacobian(const Moose::DGJacobianType type,
                                                               const unsigned int i,
                                                               const unsigned int j,
                                                               const unsigned int jvar,
                                                               const unsigned int qp,
                                                               const unsigned int neighbor_qp,
                                                               AssemblyDatum & elem_datum,
                                                               AssemblyDatum & neighbor_datum) const
{
  for (unsigned int component_j = 0; component_j < _ndisp; ++component_j)
    if (_disp_vars[component_j] == jvar)
      return computeDResidualDDisplacement(
          type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum);

  return 0;
}

KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeDResidualDDisplacement(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum) const
{
  const auto distance = surrogateDistanceVector(elem_datum, qp);
  const auto tangent = _dtraction_djump_global(elem_datum, qp)(_component, component_j);
  const auto coefficient = areaCorrection(elem_datum, qp) * tangent;

  switch (type)
  {
    case Moose::ElementElement:
      return _test(elem_datum, i, qp) * coefficient *
             shiftedPhi(_phi, _grad_phi, elem_datum, distance, j, qp);
    case Moose::ElementNeighbor:
      return -_test(elem_datum, i, qp) * coefficient *
             shiftedPhi(
                 _phi_neighbor, _grad_phi_neighbor, neighbor_datum, distance, j, neighbor_qp);
    case Moose::NeighborElement:
      return -_test_neighbor(neighbor_datum, i, neighbor_qp) * coefficient *
             shiftedPhi(_phi, _grad_phi, elem_datum, distance, j, qp);
    case Moose::NeighborNeighbor:
      return _test_neighbor(neighbor_datum, i, neighbor_qp) * coefficient *
             shiftedPhi(
                 _phi_neighbor, _grad_phi_neighbor, neighbor_datum, distance, j, neighbor_qp);
  }

  return 0;
}
