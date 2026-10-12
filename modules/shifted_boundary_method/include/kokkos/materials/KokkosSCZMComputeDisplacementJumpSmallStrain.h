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

class BoundaryShortestDistanceToSurface;

/// Computes the shifted cohesive displacement jump in global and local coordinates.
class KokkosSCZMComputeDisplacementJumpSmallStrain : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosSCZMComputeDisplacementJumpSmallStrain(const InputParameters & parameters);
  KokkosSCZMComputeDisplacementJumpSmallStrain(
      const KokkosSCZMComputeDisplacementJumpSmallStrain & object);

  void computeProperties() override;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           unsigned int qp,
                                           unsigned int neighbor_qp) const
  {
    const auto index = face.qp_map_offset + qp;
    const auto distance = _surrogate_distance_vectors[index];
    const auto rotation = _rotations[index];

    auto jump_global = _displacement_jump_global(elem_datum, qp);
    auto jump_local = _interface_displacement_jump(elem_datum, qp);
    auto property_rotation = _czm_total_rotation(elem_datum, qp);

    for (const auto i : make_range(_ndisp))
    {
      jump_global(i) =
          _displacements(neighbor_datum, neighbor_qp, i) - _displacements(elem_datum, qp, i);
      jump_global(i) += (_displacement_gradients(neighbor_datum, neighbor_qp, i) -
                         _displacement_gradients(elem_datum, qp, i))
                            .dot_product(distance);
    }

    for (const auto i : make_range(_ndisp))
    {
      jump_local(i) = 0;
      for (const auto j : make_range(_ndisp))
      {
        property_rotation(i, j) = rotation(i, j);
        jump_local(i) += rotation(j, i) * jump_global(j);
      }
    }
  }

private:
  void initializeSurrogateInterfaceMapping();

  const unsigned int _ndisp;
  const BoundaryShortestDistanceToSurface & _sbm_distance_uo;
  const Moose::Kokkos::VariableValue _displacements;
  const Moose::Kokkos::VariableGradient _displacement_gradients;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _surrogate_distance_vectors;
  Moose::Kokkos::Array<Moose::Kokkos::Real33> _rotations;
  Moose::Kokkos::MaterialProperty<Real, 1> _displacement_jump_global;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_displacement_jump;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
};
