//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosADInterfaceMaterial.h"

class BoundaryShortestDistanceToSurface;

/// Computes the AD shifted cohesive displacement jump in global and local coordinates.
class KokkosADSCZMComputeDisplacementJumpSmallStrain : public Moose::Kokkos::ADInterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosADSCZMComputeDisplacementJumpSmallStrain(const InputParameters & parameters);
  KokkosADSCZMComputeDisplacementJumpSmallStrain(
      const KokkosADSCZMComputeDisplacementJumpSmallStrain & object);

  void computeProperties() override;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           AssemblyDatum & elem_datum,
                                           AssemblyDatum & neighbor_datum,
                                           unsigned int qp,
                                           unsigned int neighbor_qp) const
  {
    const auto index = face.qp_map_offset + qp;
    const auto distance = _surrogate_distance_vectors[index];
    const auto rotation =
        _shifted ? _rotations[index] : referenceRotation(elem_datum.normals(qp), _mesh.dimension());

    auto jump_global = _displacement_jump_global(elem_datum, qp);
    auto jump_local = _interface_displacement_jump(elem_datum, qp);
    auto property_rotation = _czm_total_rotation(elem_datum, qp);

    for (unsigned int i = 0; i < _ndisp; ++i)
      jump_global(i) =
          _displacements(neighbor_datum, neighbor_qp, i) - _displacements(elem_datum, qp, i);

    if (_shifted)
      for (unsigned int i = 0; i < _ndisp; ++i)
        for (unsigned int j = 0; j < _ndisp; ++j)
          jump_global(i) += (_displacement_gradients(neighbor_datum, neighbor_qp, i)(j) -
                             _displacement_gradients(elem_datum, qp, i)(j)) *
                            distance(j);

    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      jump_local(i) = 0;
      for (unsigned int j = 0; j < _ndisp; ++j)
      {
        property_rotation(i, j) = rotation(i, j);
        jump_local(i) += rotation(j, i) * jump_global(j);
      }
    }
  }

private:
  static KOKKOS_FUNCTION Moose::Kokkos::Real33
  referenceRotation(const Moose::Kokkos::Real3 & normal, const unsigned int dim)
  {
    Moose::Kokkos::Real33 rot = 0.0;

    if (dim == 1)
    {
      rot(0, 0) = rot(1, 1) = rot(2, 2) = 1.0;
      return rot;
    }

    if (dim == 2)
    {
      const Real theta = ::Kokkos::atan2(normal(1), normal(0));
      const Real st = ::Kokkos::sin(theta);
      const Real ct = ::Kokkos::cos(theta);
      rot(0, 0) = ct;
      rot(0, 1) = -st;
      rot(1, 0) = st;
      rot(1, 1) = ct;
      rot(2, 2) = 1.0;
      return rot;
    }

    const Real nx = normal(0);
    const Real ny = normal(1);
    const Real nz = normal(2);

    Moose::Kokkos::Real3 v1;
    if (nz >= nx && nx >= ny)
    {
      v1(2) = 1.0;
      v1(0) = v1(1) = 0.0;
    }
    else if (nz >= ny && ny >= nx)
    {
      v1(1) = 1.0;
      v1(0) = v1(2) = 0.0;
    }
    else if ((nx >= nz && nz >= ny) || (nx >= ny && ny >= nz))
    {
      v1(0) = 1.0;
      v1(1) = v1(2) = 0.0;
    }
    else
    {
      v1(2) = 1.0;
      v1(0) = v1(1) = 0.0;
    }

    const Real projection = v1(0) * nx + v1(1) * ny + v1(2) * nz;
    v1(0) -= projection * nx;
    v1(1) -= projection * ny;
    v1(2) -= projection * nz;
    const Real norm = ::Kokkos::sqrt(v1(0) * v1(0) + v1(1) * v1(1) + v1(2) * v1(2));
    v1(0) /= norm;
    v1(1) /= norm;
    v1(2) /= norm;

    rot(0, 0) = v1(1) * nz - v1(2) * ny;
    rot(0, 1) = v1(2) * nx - v1(0) * nz;
    rot(0, 2) = v1(0) * ny - v1(1) * nx;
    rot(1, 0) = v1(0);
    rot(1, 1) = v1(1);
    rot(1, 2) = v1(2);
    rot(2, 0) = nx;
    rot(2, 1) = ny;
    rot(2, 2) = nz;
    return rot;
  }

  void initializeSurrogateInterfaceMapping();

  const unsigned int _ndisp;
  const bool _shifted;
  const BoundaryShortestDistanceToSurface & _sbm_distance_uo;
  const Moose::Kokkos::ADVariableValue _displacements;
  const Moose::Kokkos::ADVariableGradient _displacement_gradients;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _surrogate_distance_vectors;
  Moose::Kokkos::Array<Moose::Kokkos::Real33> _rotations;
  Moose::Kokkos::MaterialProperty<Moose::Kokkos::ADReal, 1> _displacement_jump_global;
  Moose::Kokkos::MaterialProperty<Moose::Kokkos::ADReal, 1> _interface_displacement_jump;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
};
