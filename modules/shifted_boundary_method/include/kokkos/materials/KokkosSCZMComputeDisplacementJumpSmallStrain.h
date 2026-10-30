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

    // The host uses the true normal for the rotation when shifted and the FE face normal
    // (_normals[_qp]) when no_shifted is set, see SCZMComputeDisplacementJumpBase::
    // computeRotationMatrices. _ndisp is the mesh dimension (the constructor errors unless
    // _ndisp == _mesh.dimension()) and is used instead of _mesh.dimension() because _mesh is the
    // host MooseMesh reference inherited from MaterialBase and cannot be dereferenced on device.
    const auto rotation =
        _shifted ? _rotations[index] : referenceRotation(elem_datum.normals(qp), _ndisp);

    auto jump_global = _displacement_jump_global(elem_datum, qp);
    auto jump_local = _interface_displacement_jump(elem_datum, qp);
    auto property_rotation = _czm_total_rotation(elem_datum, qp);

    for (unsigned int i = 0; i < _ndisp; ++i)
      jump_global(i) =
          _displacements(neighbor_datum, neighbor_qp, i) - _displacements(elem_datum, qp, i);

    if (_shifted)
      for (unsigned int i = 0; i < _ndisp; ++i)
        jump_global(i) += (_displacement_gradients(neighbor_datum, neighbor_qp, i) -
                           _displacement_gradients(elem_datum, qp, i))
                              .dot_product(distance);

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
  /**
   * Device equivalent of CohesiveZoneModelTools::computeReferenceRotation.
   *
   * The host helpers rotate a vector with std::atan2 and host tensor classes, neither of which is
   * callable from device code, so the same algebra is reproduced here with Kokkos math.
   */
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
      // rotVec2DToX(normal) transposed, which assumes normal(2) == 0.
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

    // rotVec1ToVec2(e_x, normal), reproduced from RotationMatrix::rotVecToZ so the device result
    // equals the host CohesiveZoneModelTools::computeReferenceRotation. rotVecToZ(normal) builds the
    // orthonormal frame with rows (v0, v1, normal), and rotVecToZ(normal)^T * rotVecToZ(e_x) works
    // out to the columns (normal, v1, -v0). The Gram-Schmidt seed is the basis vector along the
    // smallest-magnitude normal component, matching the host; it can never be parallel to the normal,
    // so the projection below cannot collapse the seed to a zero-length vector.
    Moose::Kokkos::Real3 n = normal;
    const Real n_norm = ::Kokkos::sqrt(n(0) * n(0) + n(1) * n(1) + n(2) * n(2));
    n(0) /= n_norm;
    n(1) /= n_norm;
    n(2) /= n_norm;

    const Real wx = ::Kokkos::fabs(n(0));
    const Real wy = ::Kokkos::fabs(n(1));
    const Real wz = ::Kokkos::fabs(n(2));

    Moose::Kokkos::Real3 v1 = 0.0;
    if ((wz >= wy && wy >= wx) || (wy >= wz && wz >= wx))
      v1(0) = 1.0; // n(0) is the smallest-magnitude component
    else if ((wz >= wx && wx >= wy) || (wx >= wz && wz >= wy))
      v1(1) = 1.0; // n(1) is the smallest-magnitude component
    else
      v1(2) = 1.0; // n(2) is the smallest-magnitude component

    const Real projection = v1(0) * n(0) + v1(1) * n(1) + v1(2) * n(2);
    v1(0) -= projection * n(0);
    v1(1) -= projection * n(1);
    v1(2) -= projection * n(2);
    const Real v1_norm = ::Kokkos::sqrt(v1(0) * v1(0) + v1(1) * v1(1) + v1(2) * v1(2));
    v1(0) /= v1_norm;
    v1(1) /= v1_norm;
    v1(2) /= v1_norm;

    // v0 = v1 x normal
    const Real v0x = v1(1) * n(2) - v1(2) * n(1);
    const Real v0y = v1(2) * n(0) - v1(0) * n(2);
    const Real v0z = v1(0) * n(1) - v1(1) * n(0);

    rot(0, 0) = n(0);
    rot(0, 1) = v1(0);
    rot(0, 2) = -v0x;
    rot(1, 0) = n(1);
    rot(1, 1) = v1(1);
    rot(1, 2) = -v0y;
    rot(2, 0) = n(2);
    rot(2, 1) = v1(2);
    rot(2, 2) = -v0z;
    return rot;
  }

  void initializeSurrogateInterfaceMapping();

  const unsigned int _ndisp;
  const bool _shifted;
  const BoundaryShortestDistanceToSurface & _sbm_distance_uo;
  const Moose::Kokkos::VariableValue _displacements;
  const Moose::Kokkos::VariableGradient _displacement_gradients;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _surrogate_distance_vectors;
  Moose::Kokkos::Array<Moose::Kokkos::Real33> _rotations;
  Moose::Kokkos::MaterialProperty<Real, 1> _displacement_jump_global;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_displacement_jump;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
};
