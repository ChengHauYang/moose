//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosSCZMInterfaceKernelSmallStrain.h"

/**
 * Test-only shifted interface kernel evaluating a linear cohesive traction from the shifted
 * displacement jump, and reading the dimensional traction and tangent that
 * `KokkosSCZMInterfaceKernelSmallStrain` expects from a material. The host shifted formulation in
 * this module has no counterpart of this class, because it reads those two properties from the
 * production cohesive zone model material. The Kokkos path has no such material yet, so this kernel
 * pairs with a test material instead. Substituting a linear law whose tangent is known in closed
 * form lets the consumption of those properties, the distance-based shifted values, the signed area
 * correction, and the opposing signs of the two residual rows be verified before the Kokkos
 * constitutive property pipeline exists.
 */
class KokkosLinearSCZMInterfaceKernel : public KokkosSCZMInterfaceKernelSmallStrain
{
public:
  static InputParameters validParams();

  KokkosLinearSCZMInterfaceKernel(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpResidual(Moose::DGResidualType type,
                                         unsigned int i,
                                         unsigned int qp,
                                         unsigned int neighbor_qp,
                                         AssemblyDatum & elem_datum,
                                         AssemblyDatum & neighbor_datum) const;

private:
  const Moose::Kokkos::VariableValue _displacements;
  const Moose::Kokkos::VariableGradient _displacement_gradients;
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosLinearSCZMInterfaceKernel::computeQpResidual(const Moose::DGResidualType type,
                                                   const unsigned int i,
                                                   const unsigned int qp,
                                                   const unsigned int neighbor_qp,
                                                   AssemblyDatum & elem_datum,
                                                   AssemblyDatum & neighbor_datum) const
{
  const auto distance = surrogateDistanceVector(elem_datum, qp);
  Real traction = 0;
  const auto tangent = _dtraction_djump_global(elem_datum, qp);

  for (unsigned int component_j = 0; component_j < _ndisp; ++component_j)
  {
    const auto elem_value =
        _displacements(elem_datum, qp, component_j) +
        _displacement_gradients(elem_datum, qp, component_j).dot_product(distance);
    const auto neighbor_value =
        _displacements(neighbor_datum, neighbor_qp, component_j) +
        _displacement_gradients(neighbor_datum, neighbor_qp, component_j).dot_product(distance);
    traction += tangent(_component, component_j) * (neighbor_value - elem_value);
  }

  const auto test = type == Moose::Element ? _test(elem_datum, i, qp)
                                           : _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto sign = type == Moose::Element ? -1.0 : 1.0;
  return sign * test * areaCorrection(elem_datum, qp) * traction;
}
