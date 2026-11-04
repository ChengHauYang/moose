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

    auto prop_t_global = _traction_global(elem_datum, qp);
    auto prop_PK1 = _PK1traction(elem_datum, qp);
    auto prop_dt_djump_global = _dtraction_djump_global(elem_datum, qp);
    auto prop_dPK1_dF = _dPK1traction_dF(elem_datum, qp);

    Moose::Kokkos::Real33 F = 0.0;
    F(0, 0) = 1.0;
    F(1, 1) = 1.0;
    F(2, 2) = 1.0;
    Moose::Kokkos::Real33 R = 0.0;
    R(0, 0) = 1.0;
    R(1, 1) = 1.0;
    R(2, 2) = 1.0;
    Moose::Kokkos::Real33 Rref = 0.0;
    Rref(0, 0) = 1.0;
    Rref(1, 1) = 1.0;
    Rref(2, 2) = 1.0;
    Moose::Kokkos::Real33 Rtot = 0.0;
    Rtot(0, 0) = 1.0;
    Rtot(1, 1) = 1.0;
    Rtot(2, 2) = 1.0;

    Real jump_global[3] = {0, 0, 0};
    Real t_local[3] = {0, 0, 0};
    Real dt_local[3][3] = {{0}};

    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      jump_global[i] = prop_jump_global(i);
      t_local[i] = prop_t_local(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
      {
        F(i, j) = prop_F(i, j);
        R(i, j) = prop_R(i, j);
        Rref(i, j) = prop_Rref(i, j);
        Rtot(i, j) = prop_Rtot(i, j);
        dt_local[i][j] = prop_dt_local(i, j);
      }
    }

    Moose::Kokkos::Real3 N = normal_ref;
    if (_ndisp < 3)
      N(2) = 0.0;

    const Moose::Kokkos::Real33 U = R.transpose() * F;
    const Real J = F.determinant(3);
    const Moose::Kokkos::Real33 FinvT = F.inverse(3).transpose();

    const Moose::Kokkos::Real3 Fitr_N = FinvT * N;
    const Real Fitr_N_norm = Fitr_N.norm();
    const Real area_ratio = J * Fitr_N_norm;

    // Cauchy-like global traction: t_global = R_tot * t_local
    Real t_global[3] = {0, 0, 0};
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      for (unsigned int j = 0; j < _ndisp; ++j)
        t_global[i] += Rtot(i, j) * t_local[j];
      prop_t_global(i) = t_global[i];
      prop_PK1(i) = t_global[i] * area_ratio;
    }

    // dtraction_djump_global = area_ratio * R_tot * dt_local * R_tot^T
    for (unsigned int i = 0; i < _ndisp; ++i)
      for (unsigned int j = 0; j < _ndisp; ++j)
      {
        Real sum = 0.0;
        for (unsigned int k = 0; k < _ndisp; ++k)
          for (unsigned int l = 0; l < _ndisp; ++l)
            sum += Rtot(i, k) * dt_local[k][l] * Rtot(j, l);
        prop_dt_djump_global(i, j) = area_ratio * sum;
      }

    Real dFinv[3][3][3][3];
    Moose::Kokkos::CohesiveZoneModelTools::computedFinversedF(FinvT, dFinv);

    Real dArea_dF[3][3];
    Moose::Kokkos::CohesiveZoneModelTools::computeDAreaRatioDF(FinvT, N, J, dFinv, dArea_dF);

    Real dR_dF[3][3][3][3];
    Moose::Kokkos::CohesiveZoneModelTools::computedRdF(R, U, dR_dF);

    Real dczm_tot_rot_dF[3][3][3][3] = {{{{0}}}};
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
        for (unsigned int k = 0; k < 3; ++k)
          for (unsigned int l = 0; l < 3; ++l)
            for (unsigned int m = 0; m < 3; ++m)
              dczm_tot_rot_dF[i][j][k][l] += Rref(m, j) * dR_dF[i][m][k][l];

    Real djump_dF[3][3][3] = {{{0}}};
    for (unsigned int a = 0; a < 3; ++a)
      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int l = 0; l < 3; ++l)
          for (unsigned int b = 0; b < 3; ++b)
            djump_dF[a][k][l] += dczm_tot_rot_dF[b][a][k][l] * jump_global[b];

    Real M[3][3] = {{0}};
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
        for (unsigned int k = 0; k < 3; ++k)
          M[i][j] += Rtot(i, k) * dt_local[k][j];

    for (unsigned int i = 0; i < _ndisp; ++i)
      for (unsigned int k = 0; k < _ndisp; ++k)
        for (unsigned int l = 0; l < _ndisp; ++l)
        {
          const Real term1 = dArea_dF[k][l] * t_global[i];
          Real term2 = 0.0;
          for (unsigned int j = 0; j < 3; ++j)
            term2 += dczm_tot_rot_dF[i][j][k][l] * t_local[j];
          Real term3 = 0.0;
          for (unsigned int j = 0; j < 3; ++j)
            term3 += M[i][j] * djump_dF[j][k][l];

          prop_dPK1_dF(i, k, l) = term1 + area_ratio * (term2 + term3);
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
