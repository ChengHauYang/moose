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
#include "SCZMCohesiveLawEvaluators.h"
#include "SCZMGlobalTractionTotalLagrangian.h"

class BoundaryShortestDistanceToSurface;

/**
 * Computes total-Lagrangian PK1 interface traction and its derivatives w.r.t. displacement jump
 * and deformation gradient with Kokkos.
 */
class KokkosSCZMComputeGlobalTractionTotalLagrangian : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosSCZMComputeGlobalTractionTotalLagrangian(const InputParameters & parameters);
  KokkosSCZMComputeGlobalTractionTotalLagrangian(
      const KokkosSCZMComputeGlobalTractionTotalLagrangian & object);

  void computeProperties() override;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum &,
                                           unsigned int qp,
                                           unsigned int) const
  {
    const auto index = face.qp_map_offset + qp;
    const auto normal_ref = _shifted ? _true_normals[index] : elem_datum.normals(qp);
    const auto prop_jump_global = _displacement_jump_global(elem_datum, qp);
    const auto prop_Rtot = _czm_total_rotation(elem_datum, qp);
    const auto prop_Rref = _czm_reference_rotation(elem_datum, qp);
    const auto prop_R = _czm_rotation(elem_datum, qp);
    const auto prop_F = _F_czm(elem_datum, qp);
    const auto prop_t_local = _interface_traction(elem_datum, qp);
    const auto prop_dt_local = _dinterface_traction_djump(elem_datum, qp);

    Moose::Kokkos::SCZM::GlobalTractionTotalLagrangian<Moose::Kokkos::SCZM::AffineCohesiveLaw>
        evaluator;
    evaluator.ndisp = _ndisp;
    for (unsigned int i = 0; i < 3; ++i)
    {
      evaluator.deformation_gradient(i, i) = 1.0;
      evaluator.rotation(i, i) = 1.0;
      evaluator.reference_rotation(i, i) = 1.0;
      evaluator.total_rotation(i, i) = 1.0;
    }

    Moose::Kokkos::Real3 global_jump{};
    Moose::Kokkos::Real3 local_jump{};
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      global_jump(i) = prop_jump_global(i);
      evaluator.reference_normal(i) = normal_ref(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
      {
        evaluator.deformation_gradient(i, j) = prop_F(i, j);
        evaluator.rotation(i, j) = prop_R(i, j);
        evaluator.reference_rotation(i, j) = prop_Rref(i, j);
        evaluator.total_rotation(i, j) = prop_Rtot(i, j);
        evaluator.local_law.stiffness(i, j) = prop_dt_local(i, j);
        local_jump(i) += prop_Rtot(j, i) * prop_jump_global(j);
      }
    }
    if (_ndisp < 3)
      evaluator.reference_normal(2) = 0.0;

    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      evaluator.local_law.offset(i) = prop_t_local(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
        evaluator.local_law.offset(i) -= evaluator.local_law.stiffness(i, j) * local_jump(j);
    }

    const auto evaluation =
        evaluator.evaluateWithDeformationGradientDerivative(global_jump, _ndisp);
    auto prop_t_global = _traction_global(elem_datum, qp);
    auto prop_PK1 = _PK1traction(elem_datum, qp);
    auto prop_dt_djump_global = _dtraction_djump_global(elem_datum, qp);
    auto prop_dPK1_dF = _dPK1traction_dF(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      prop_t_global(i) = evaluation.cauchy_traction(i);
      prop_PK1(i) = evaluation.cohesive.traction(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
      {
        prop_dt_djump_global(i, j) = evaluation.cohesive.jump_tangent(i, j);
        for (unsigned int k = 0; k < _ndisp; ++k)
          prop_dPK1_dF(i, j, k) = evaluation.dtraction_dF[i][j][k];
      }
    }
  }

private:
  void initializeSurrogateInterfaceMapping();

  const unsigned int _ndisp;
  const bool _shifted;
  const BoundaryShortestDistanceToSurface * _sbm_distance_uo;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _true_normals;
  Moose::Kokkos::MaterialProperty<Real, 1> _displacement_jump_global;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_reference_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _F_czm;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_traction;
  Moose::Kokkos::MaterialProperty<Real, 2> _dinterface_traction_djump;
  Moose::Kokkos::MaterialProperty<Real, 1> _traction_global;
  Moose::Kokkos::MaterialProperty<Real, 1> _PK1traction;
  Moose::Kokkos::MaterialProperty<Real, 2> _dtraction_djump_global;
  Moose::Kokkos::MaterialProperty<Real, 3> _dPK1traction_dF;
};
