//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosInterfaceUserObject.h"
#include "KokkosMaterialPropertyValue.h"
#include "NEML2BatchIndexGenerator.h"
#include "SCZMCohesiveLawEvaluators.h"
#include "SCZMGeneralizedRobinSolver.h"
#include "SCZMGlobalTractionTotalLagrangian.h"
#include "SCZMWeightedTractionStabilization.h"

class BoundaryShortestDistanceToSurface;

/**
 * Computes the bulk-side data shared by weighted-traction SCZM kernels and NEML2 executors.
 *
 * Data are stored in interface-face/QP order using InterfaceFaceRecord::qp_map_offset. This keeps
 * the prepass independent of material-property ownership and gives host NEML2 and device kernels a
 * single canonical source for bulk traction, stabilization beta, geometry, and recovery weights.
 */
class KokkosSCZMStabilizationData : public Moose::Kokkos::InterfaceUserObject
{
public:
  static InputParameters validParams();

  KokkosSCZMStabilizationData(const InputParameters & parameters);
  KokkosSCZMStabilizationData(const KokkosSCZMStabilizationData & object);

  using InterfaceUserObject::operator();

  void initialSetup() override;
  void initialize() override {}
  void compute() override {}
  void finalize() override {}

  void invalidateAssemblyCache();
  void ensurePrepared(bool need_tangents);

  template <typename Derived>
  KOKKOS_FUNCTION void executeInterface(Datum & primary,
                                        Datum & neighbor,
                                        const Moose::Kokkos::InterfaceFaceRecord & face) const;

  KOKKOS_FUNCTION const Moose::Kokkos::Real3 &
  bulkTraction(const Moose::Kokkos::InterfaceFaceRecord & face, unsigned int qp) const
  {
    return _bulk_traction[face.qp_map_offset + qp];
  }

  KOKKOS_FUNCTION Real stabilizationBeta(const Moose::Kokkos::InterfaceFaceRecord & face,
                                         unsigned int qp) const
  {
    return _settings.beta_override ? _settings.beta : _stabilization_beta[face.qp_map_offset + qp];
  }

  KOKKOS_FUNCTION Moose::Kokkos::Real3 trueNormal(const Moose::Kokkos::InterfaceFaceRecord & face,
                                                  unsigned int qp) const
  {
    return _shifted ? _true_normals[face.qp_map_offset + qp] : Moose::Kokkos::Real3{};
  }

  KOKKOS_FUNCTION dof_id_type recoveryOffset(const Moose::Kokkos::InterfaceFaceRecord & face,
                                             unsigned int qp,
                                             bool on_primary,
                                             bool at_surrogate = false) const
  {
    return _recovery_offsets[4 * (face.qp_map_offset + qp) + 2 * at_surrogate +
                             (on_primary ? 0 : 1)];
  }

  KOKKOS_FUNCTION const Moose::Kokkos::Array<Real> & recoveryWeights() const
  {
    return _recovery_weights;
  }

  const Moose::Kokkos::Array<Moose::Kokkos::Real3> & bulkTractions() const
  {
    return _bulk_traction;
  }

  const Moose::Kokkos::Array<Real> & stabilizationBetas() const { return _stabilization_beta; }

  const Moose::Kokkos::Array<dof_id_type> & recoveryOffsets() const { return _recovery_offsets; }

  const Moose::Kokkos::Array<Real> & hostRecoveryWeights() const { return _recovery_weights; }
  const Moose::Kokkos::Array2D<dof_id_type> & faceQpOffsets() const { return _face_qp_offsets; }
  const Moose::Kokkos::Array<Moose::Kokkos::Real3> & stabilizedTractions() const
  {
    return _stabilized_traction;
  }
  const Moose::Kokkos::Array<Moose::Kokkos::Real3> & auxiliaryJumps() const
  {
    return _auxiliary_jump;
  }
  const Moose::Kokkos::Array<Moose::Kokkos::Real33> & robinTangents() const { return _robin_W; }
  const Moose::Kokkos::Array<Moose::Kokkos::Real33> & inverseRobinTangents() const
  {
    return _inverse_robin_tangent;
  }
  const Moose::Kokkos::Array2D<Real> & robinWeightedDeformationGradientDerivatives() const
  {
    return _robin_weighted_dF;
  }
  std::size_t numFaces() const { return kokkosInterfaceFaces().size(); }
  dof_id_type numQps() const { return _bulk_traction.size(); }

protected:
  void initializeSurrogateInterfaceMapping();
  void allocateArrays();
  void initializeRecovery();
  void validateCanonicalBatchOwnership();

  const unsigned int _ndisp;
  const bool _shifted;
  const bool _element_linear_recovery;
  const bool _coupled_cubic_stabilization;
  const Real _stabilization_linear_stiffness;
  const Real _stabilization_cubic_stiffness;
  const SCZMWeightedTractionStabilization::Settings _settings;
  const BoundaryShortestDistanceToSurface * const _sbm_distance_uo;
  const NEML2BatchIndexGenerator * const _batch_index_generator;

  Moose::Kokkos::Array<Moose::Kokkos::Real3> _true_normals;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _surrogate_distance_vectors;
  Moose::Kokkos::Array2D<dof_id_type> _face_qp_offsets;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _bulk_traction;
  Moose::Kokkos::Array<Real> _stabilization_beta;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _stabilized_traction;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _auxiliary_jump;
  Moose::Kokkos::Array<Moose::Kokkos::Real33> _robin_W;
  Moose::Kokkos::Array<Moose::Kokkos::Real33> _inverse_robin_tangent;
  Moose::Kokkos::Array2D<Real> _robin_weighted_dF;
  Moose::Kokkos::Array<dof_id_type> _recovery_offsets;
  Moose::Kokkos::Array<Real> _recovery_weights;

  Moose::Kokkos::MaterialProperty<Real, 1> _displacement_jump_global;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_reference_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_rotation;
  Moose::Kokkos::MaterialProperty<Real, 2> _F_czm;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_traction;
  Moose::Kokkos::MaterialProperty<Real, 2> _dinterface_traction_djump;
  Moose::Kokkos::MaterialProperty<Real, 2> _stress;
  Moose::Kokkos::MaterialProperty<Real, 2> _stress_neighbor;
  Moose::Kokkos::MaterialProperty<Real, 2> _volume_stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _volume_tangent;

  std::unique_ptr<Moose::Kokkos::DispatcherBase> _prepass_dispatcher;
  bool _mapping_ready = false;
  bool _recovery_initialized = false;
  bool _beta_initialized = false;
  bool _bulk_and_robin_valid = false;
  bool _robin_tangents_valid = false;
  bool _need_tangents = false;
  bool _initialize_beta_only = false;
  Moose::Kokkos::Array<int> _prepass_failure;
  std::size_t _validated_map_generation = 0;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosSCZMStabilizationData::executeInterface(Datum & primary,
                                              Datum & neighbor,
                                              const Moose::Kokkos::InterfaceFaceRecord & face) const
{
  Datum primary_volume(
      face.elem, libMesh::invalid_uint, face.neighbor, kokkosAssembly(), kokkosSystems(), true);
  Datum neighbor_volume(
      face.neighbor, libMesh::invalid_uint, face.elem, kokkosAssembly(), kokkosSystems(), true);

  Real face_area = 0.0;
  Real primary_volume_measure = 0.0;
  Real neighbor_volume_measure = 0.0;
  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
    face_area += primary.JxW(qp);
  for (unsigned int qp = 0; qp < primary_volume.n_qps(); ++qp)
    primary_volume_measure += primary_volume.JxW(qp);
  for (unsigned int qp = 0; qp < neighbor_volume.n_qps(); ++qp)
    neighbor_volume_measure += neighbor_volume.JxW(qp);

  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
  {
    const auto index = face.qp_map_offset + qp;
    const auto neighbor_qp = kokkosInterfaceNeighborQp(face, qp);
    const auto normal = _shifted ? _true_normals[index] : primary.normals(qp);
    Moose::Kokkos::Real3 average_traction{};
    for (unsigned int i = 0; i < _ndisp; ++i)
      for (unsigned int J = 0; J < _ndisp; ++J)
      {
        Real primary_stress = 0.0;
        Real neighbor_stress = 0.0;
        if (_element_linear_recovery)
        {
          const auto primary_offset = recoveryOffset(face, qp, true);
          const auto neighbor_offset = recoveryOffset(face, qp, false);
          for (unsigned int volume_qp = 0; volume_qp < primary_volume.n_qps(); ++volume_qp)
            primary_stress += _recovery_weights[primary_offset + volume_qp] *
                              _volume_stress(primary_volume, volume_qp)(i, J);
          for (unsigned int volume_qp = 0; volume_qp < neighbor_volume.n_qps(); ++volume_qp)
            neighbor_stress += _recovery_weights[neighbor_offset + volume_qp] *
                               _volume_stress(neighbor_volume, volume_qp)(i, J);
        }
        else
        {
          primary_stress = _stress(primary, qp)(i, J);
          neighbor_stress = _stress_neighbor(neighbor, neighbor_qp)(i, J);
        }
        average_traction(i) += 0.5 * (primary_stress + neighbor_stress) * normal(J);
      }
    _bulk_traction[index] = average_traction;

    if (!_settings.beta_override && !_beta_initialized)
    {
      Real primary_stiffness = 0.0;
      Real neighbor_stiffness = 0.0;
      for (unsigned int i = 0; i < _ndisp; ++i)
        for (unsigned int J = 0; J < _ndisp; ++J)
          for (unsigned int k = 0; k < _ndisp; ++k)
            for (unsigned int L = 0; L < _ndisp; ++L)
              if (_element_linear_recovery)
              {
                const auto primary_offset = recoveryOffset(face, qp, true);
                const auto neighbor_offset = recoveryOffset(face, qp, false);
                for (unsigned int volume_qp = 0; volume_qp < primary_volume.n_qps(); ++volume_qp)
                  primary_stiffness += normal(i) * normal(J) * normal(k) * normal(L) *
                                       _recovery_weights[primary_offset + volume_qp] *
                                       _volume_tangent(primary_volume, volume_qp)(i, J, k, L);
                for (unsigned int volume_qp = 0; volume_qp < neighbor_volume.n_qps(); ++volume_qp)
                  neighbor_stiffness += normal(i) * normal(J) * normal(k) * normal(L) *
                                        _recovery_weights[neighbor_offset + volume_qp] *
                                        _volume_tangent(neighbor_volume, volume_qp)(i, J, k, L);
              }
              else
              {
                for (unsigned int volume_qp = 0; volume_qp < primary_volume.n_qps(); ++volume_qp)
                  primary_stiffness += normal(i) * normal(J) * normal(k) * normal(L) *
                                       primary_volume.JxW(volume_qp) / primary_volume_measure *
                                       _volume_tangent(primary_volume, volume_qp)(i, J, k, L);
                for (unsigned int volume_qp = 0; volume_qp < neighbor_volume.n_qps(); ++volume_qp)
                  neighbor_stiffness += normal(i) * normal(J) * normal(k) * normal(L) *
                                        neighbor_volume.JxW(volume_qp) / neighbor_volume_measure *
                                        _volume_tangent(neighbor_volume, volume_qp)(i, J, k, L);
              }
      _stabilization_beta[index] = 0.5 * _settings.gamma * face_area *
                                   (primary_stiffness / primary_volume_measure +
                                    neighbor_stiffness / neighbor_volume_measure);
    }
    if (_initialize_beta_only)
      continue;

    Moose::Kokkos::SCZM::GlobalTractionTotalLagrangian<Moose::Kokkos::SCZM::NativeCohesiveLaw>
        evaluator;
    evaluator.local_law.coupled_cubic = _coupled_cubic_stabilization;
    evaluator.local_law.cubic.linear_stiffness = _stabilization_linear_stiffness;
    evaluator.local_law.cubic.cubic_stiffness = _stabilization_cubic_stiffness;
    evaluator.ndisp = _ndisp;
    for (unsigned int i = 0; i < 3; ++i)
    {
      evaluator.deformation_gradient(i, i) = 1.0;
      evaluator.rotation(i, i) = 1.0;
      evaluator.reference_rotation(i, i) = 1.0;
      evaluator.total_rotation(i, i) = 1.0;
    }

    const auto jump_property = _displacement_jump_global(primary, qp);
    const auto total_rotation = _czm_total_rotation(primary, qp);
    const auto reference_rotation = _czm_reference_rotation(primary, qp);
    const auto rotation = _czm_rotation(primary, qp);
    const auto deformation_gradient = _F_czm(primary, qp);
    const auto local_traction = _interface_traction(primary, qp);
    const auto local_tangent = _dinterface_traction_djump(primary, qp);
    Moose::Kokkos::Real3 jump{};
    Moose::Kokkos::Real3 local_jump{};
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      jump(i) = jump_property(i);
      evaluator.reference_normal(i) = normal(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
      {
        evaluator.deformation_gradient(i, j) = deformation_gradient(i, j);
        evaluator.rotation(i, j) = rotation(i, j);
        evaluator.reference_rotation(i, j) = reference_rotation(i, j);
        evaluator.total_rotation(i, j) = total_rotation(i, j);
        evaluator.local_law.affine.stiffness(i, j) = local_tangent(i, j);
        local_jump(i) += total_rotation(j, i) * jump_property(j);
      }
    }
    if (_ndisp < 3)
      evaluator.reference_normal(2) = 0.0;
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      evaluator.local_law.affine.offset(i) = local_traction(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
        evaluator.local_law.affine.offset(i) -= local_tangent(i, j) * local_jump(j);
    }

    const Real beta = _settings.beta_override ? _settings.beta : _stabilization_beta[index];
    const auto stabilized = Moose::Kokkos::SCZM::GeneralizedRobinSolver::solve(
        evaluator, jump, average_traction, beta, _ndisp, !_coupled_cubic_stabilization);
    if (!stabilized.converged)
    {
      ::Kokkos::atomic_exchange(&_prepass_failure[0], 1);
      continue;
    }
    _stabilized_traction[index] = stabilized.traction;
    _auxiliary_jump[index] = stabilized.auxiliary_jump;
    if (_need_tangents)
    {
      _robin_W[index] = stabilized.dtraction_djump;
      _inverse_robin_tangent[index] = stabilized.inverse_robin_tangent;
      const auto auxiliary =
          evaluator.evaluateWithDeformationGradientDerivative(stabilized.auxiliary_jump, _ndisp);
      for (unsigned int a = 0; a < _ndisp; ++a)
        for (unsigned int k = 0; k < _ndisp; ++k)
          for (unsigned int l = 0; l < _ndisp; ++l)
          {
            Real weighted = 0.0;
            for (unsigned int b = 0; b < _ndisp; ++b)
              weighted += stabilized.inverse_robin_tangent(a, b) * auxiliary.dtraction_dF[b][k][l];
            _robin_weighted_dF(index, 9 * a + 3 * k + l) = weighted;
          }
    }
  }
}
