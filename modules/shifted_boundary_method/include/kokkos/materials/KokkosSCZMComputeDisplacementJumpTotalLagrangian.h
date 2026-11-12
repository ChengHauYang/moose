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
#include "KokkosCohesiveZoneModelTools.h"

class BoundaryShortestDistanceToSurface;

/**
 * Computes the shifted cohesive displacement jump and deformation gradient quantities
 * in global and local coordinates for total-Lagrangian kinematics with Kokkos.
 */
class KokkosSCZMComputeDisplacementJumpTotalLagrangian : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosSCZMComputeDisplacementJumpTotalLagrangian(const InputParameters & parameters);
  KokkosSCZMComputeDisplacementJumpTotalLagrangian(
      const KokkosSCZMComputeDisplacementJumpTotalLagrangian & object);

  void computeProperties() override;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           unsigned int qp,
                                           unsigned int neighbor_qp) const
  {
    const auto index = face.qp_map_offset + qp;
    const auto distance = _shifted ? _surrogate_distance_vectors[index] : Moose::Kokkos::Real3(0);

    // Compute reference rotation R_ref
    Moose::Kokkos::Real33 rot_ref = 0.0;
    if (_shifted)
      rot_ref = _rotations[index];
    else
      rot_ref =
          Moose::Kokkos::CohesiveZoneModelTools::referenceRotation(elem_datum.normals(qp), _ndisp);

    // Displacement jump in global coordinates: jump_global = u_neigh - u_elem + (grad_u_neigh -
    // grad_u_elem) . d
    auto jump_global = _displacement_jump_global(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
      jump_global(i) =
          _displacements(neighbor_datum, neighbor_qp, i) - _displacements(elem_datum, qp, i);

    if (_shifted)
      for (unsigned int i = 0; i < _ndisp; ++i)
        jump_global(i) += (_displacement_gradients(neighbor_datum, neighbor_qp, i) -
                           _displacement_gradients(elem_datum, qp, i))
                              .dot_product(distance);

    // Deformation gradient on interface F = 0.5 * (F_elem + F_neighbor)
    Moose::Kokkos::Real33 F = 0.0;
    F(0, 0) = 1.0;
    F(1, 1) = 1.0;
    F(2, 2) = 1.0;
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      const auto grad_elem = _displacement_gradients(elem_datum, qp, i);
      const auto grad_neigh = _displacement_gradients(neighbor_datum, neighbor_qp, i);
      for (unsigned int j = 0; j < 3; ++j)
        F(i, j) += 0.5 * (grad_elem(j) + grad_neigh(j));
    }

    // Polar decomposition of F: F = R * U
    Moose::Kokkos::Real33 R = 0.0;
    Moose::Kokkos::Real33 U = 0.0;
    Moose::Kokkos::Real33 Uinv = 0.0;
    Moose::Kokkos::CohesiveZoneModelTools::polarDecomposition3x3(F, R, U, Uinv);

    // Total rotation R_total = R * R_ref
    const Moose::Kokkos::Real33 R_tot = R * rot_ref;

    // Interface displacement jump in local coordinates: jump_local = R_total^T * jump_global
    auto jump_local = _interface_displacement_jump(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      jump_local(i) = 0.0;
      for (unsigned int j = 0; j < _ndisp; ++j)
        jump_local(i) += R_tot(j, i) * jump_global(j);
    }

    // Assign properties
    auto prop_F = _F_czm(elem_datum, qp);
    auto prop_R = _czm_rotation(elem_datum, qp);
    auto prop_Rref = _czm_reference_rotation(elem_datum, qp);
    auto prop_Rtot = _czm_total_rotation(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
      for (unsigned int j = 0; j < _ndisp; ++j)
      {
        prop_F(i, j) = F(i, j);
        prop_R(i, j) = R(i, j);
        prop_Rref(i, j) = rot_ref(i, j);
        prop_Rtot(i, j) = R_tot(i, j);
      }
  }

private:
  void initializeSurrogateInterfaceMapping();

  const unsigned int _ndisp;
  const bool _shifted;
  const BoundaryShortestDistanceToSurface * _sbm_distance_uo;
  const Moose::Kokkos::VariableValue _displacements;
  const Moose::Kokkos::VariableGradient _displacement_gradients;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _surrogate_distance_vectors;
  Moose::Kokkos::Array<Moose::Kokkos::Real33> _rotations;
  Moose::Kokkos::MaterialProperty<Real, 1> _displacement_jump_global;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_displacement_jump;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_reference_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _F_czm;
};
