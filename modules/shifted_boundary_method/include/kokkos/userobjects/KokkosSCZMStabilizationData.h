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
#include "SCZMCohesiveLawEvaluators.h"
#include "SCZMGeneralizedRobinSolver.h"
#include "SCZMGlobalTractionTotalLagrangian.h"
#include "SCZMTrialTractionProvider.h"
#include "SCZMWeightedTractionStabilization.h"

#include <type_traits>
#include <utility>

class BoundaryShortestDistanceToSurface;

namespace Moose::Kokkos::SCZM
{
/**
 * Detects a trial law material with recordTrialState(datum, qp, local_jump), which records the
 * state of a stateful law at the converged local auxiliary jump of the generalized Robin solve
 */
template <typename TSL, typename DatumType, typename = void>
struct RecordsTrialState : std::false_type
{
};

template <typename TSL, typename DatumType>
struct RecordsTrialState<TSL,
                         DatumType,
                         std::void_t<decltype(std::declval<const TSL &>().recordTrialState(
                             std::declval<DatumType &>(), 0u, std::declval<const Real3 &>()))>>
  : std::true_type
{
};
} // namespace Moose::Kokkos::SCZM

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

  /// Launches the generalized Robin solve with the trial law of the traction material tsl
  template <typename TSL>
  void robinPrepass(const TSL & tsl) const;

  template <typename TSL>
  KOKKOS_FUNCTION void robinFace(const TSL & tsl, Moose::Kokkos::ThreadID tid) const;

  /// Per-QP inputs of the generalized Robin solve in interface-face/QP order, for host trial laws
  struct RobinInputs
  {
    Moose::Kokkos::Array<Moose::Kokkos::Real3> shifted_jump;
    Moose::Kokkos::Array<Moose::Kokkos::Real3> bulk_traction;
    Moose::Kokkos::Array<Moose::Kokkos::Real3> reference_normal;
    Moose::Kokkos::Array<Real> beta;
    Moose::Kokkos::Array<Moose::Kokkos::Real33> total_rotation;
    /// Identity in small strain
    Moose::Kokkos::Array<Moose::Kokkos::Real33> reference_rotation;
    Moose::Kokkos::Array<Moose::Kokkos::Real33> rotation;
    Moose::Kokkos::Array<Moose::Kokkos::Real33> deformation_gradient;
  };

  /// Per-QP results of a host generalized Robin solve in interface-face/QP order
  struct RobinResults
  {
    Moose::Kokkos::Array<Moose::Kokkos::Real3> traction;
    Moose::Kokkos::Array<Moose::Kokkos::Real3> auxiliary_jump;
    Moose::Kokkos::Array<Moose::Kokkos::Real33> robin_W;
    Moose::Kokkos::Array<Moose::Kokkos::Real33> inverse_robin_tangent;
    /// S-weighted d(traction)/d(F), laid out as _robin_weighted_dF; finite strain only
    Moose::Kokkos::Array2D<Real> robin_weighted_dF;
    /// Nonzero where the local solve converged
    Moose::Kokkos::Array<int> converged;
  };

  /// Gathers the Robin inputs of every interface QP on device and mirrors them on host
  void gatherRobinInputs(RobinInputs & inputs) const;
  KOKKOS_FUNCTION void gatherRobinInputsFace(const RobinInputs & inputs,
                                             Moose::Kokkos::ThreadID tid) const;

  /// Stores device-resident host Robin results, flagging a prepass failure for unconverged QPs
  void storeRobinResults(const RobinResults & results) const;
  KOKKOS_FUNCTION void storeRobinResult(const RobinResults & results, dof_id_type index) const;

  KOKKOS_FUNCTION const Moose::Kokkos::Real3 &
  bulkTraction(const Moose::Kokkos::InterfaceFaceRecord & face, unsigned int qp) const
  {
    return _bulk_traction[face.qp_map_offset + qp];
  }

  KOKKOS_FUNCTION Real stabilizationBeta(const Moose::Kokkos::InterfaceFaceRecord & face,
                                         unsigned int qp) const
  {
    return _stabilization_beta(face.qp_map_offset + qp);
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
  const Moose::Kokkos::Array<Real> & primaryWeights() const { return _primary_weight; }

  const SCZMWeightedTractionStabilization::Settings & stabilizationSettings() const
  {
    return _settings;
  }
  bool shifted() const { return _shifted; }
  bool finiteStrain() const { return _finite_strain; }
  bool elementLinearRecovery() const { return _element_linear_recovery; }

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
  const Moose::Kokkos::Array<Moose::Kokkos::InterfaceFaceRecord> & interfaceFaces() const
  {
    return kokkosInterfaceFaces();
  }
  unsigned int numDisplacements() const { return _ndisp; }

protected:
  void initializeSurrogateInterfaceMapping();
  void allocateArrays();
  void findTrialTractionProvider();
  void initializeRecovery();

  const unsigned int _ndisp;
  /// Total-Lagrangian kinematics; small strain keeps the identity deformation gradient and rotation
  const bool _finite_strain;
  const bool _shifted;
  const bool _element_linear_recovery;
  const SCZMWeightedTractionStabilization::Settings _settings;
  const BoundaryShortestDistanceToSurface * const _sbm_distance_uo;

  Moose::Kokkos::Array<Moose::Kokkos::Real3> _true_normals;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _surrogate_distance_vectors;
  Moose::Kokkos::Array2D<dof_id_type> _face_qp_offsets;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _bulk_traction;
  Moose::Kokkos::Array<Real> _stabilization_beta;
  /// Host copy of the stabilization beta, which carries it through restart and recovery
  /// because restartable data is restored before initialSetup sizes the device array
  std::vector<Real> & _restartable_stabilization_beta;
  /// Frozen weight of the primary side traction in the average bulk traction
  Moose::Kokkos::Array<Real> _primary_weight;
  /// Host copy of the primary side weight, restored with the stabilization beta
  std::vector<Real> & _restartable_primary_weight;
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
  Moose::Kokkos::MaterialProperty<Real, 2> _stress;
  Moose::Kokkos::MaterialProperty<Real, 2> _stress_neighbor;
  /// Face bulk tangent on each side, for the stabilization beta without recovery
  Moose::Kokkos::MaterialProperty<Real, 4> _tangent;
  Moose::Kokkos::MaterialProperty<Real, 4> _tangent_neighbor;
  Moose::Kokkos::MaterialProperty<Real, 2> _volume_stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _volume_tangent;

  std::unique_ptr<Moose::Kokkos::DispatcherBase> _prepass_dispatcher;
  /// Traction-separation material on this interface that evaluates the cohesive law at trial jumps
  const SCZMTrialTractionProvider * _trial_provider = nullptr;
  /// Owns _trial_provider when the law is not itself a provider, e.g. a NEML2 model
  std::unique_ptr<SCZMTrialTractionProvider> _owned_trial_provider;
  bool _mapping_ready = false;
  bool _recovery_initialized = false;
  Moose::Kokkos::Scalar<bool> _beta_initialized;
  bool _bulk_and_robin_valid = false;
  bool _robin_tangents_valid = false;
  bool _need_tangents = false;
  Moose::Kokkos::Array<int> _prepass_failure;
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

    if (!_beta_initialized)
    {
      // The normal stiffness is taken from the same tangent as the bulk traction: the recovered
      // volume tangent with recovery, else the face tangent on each side
      Real primary_stiffness = 0.0;
      Real neighbor_stiffness = 0.0;
      if (_element_linear_recovery)
      {
        const auto primary_offset = recoveryOffset(face, qp, true);
        const auto neighbor_offset = recoveryOffset(face, qp, false);
        for (unsigned int volume_qp = 0; volume_qp < primary_volume.n_qps(); ++volume_qp)
          primary_stiffness += _recovery_weights[primary_offset + volume_qp] *
                               Moose::Kokkos::SCZM::normalStiffness(
                                   _volume_tangent(primary_volume, volume_qp), normal, _ndisp);
        for (unsigned int volume_qp = 0; volume_qp < neighbor_volume.n_qps(); ++volume_qp)
          neighbor_stiffness += _recovery_weights[neighbor_offset + volume_qp] *
                                Moose::Kokkos::SCZM::normalStiffness(
                                    _volume_tangent(neighbor_volume, volume_qp), normal, _ndisp);
      }
      else
      {
        primary_stiffness =
            Moose::Kokkos::SCZM::normalStiffness(_tangent(primary, qp), normal, _ndisp);
        neighbor_stiffness = Moose::Kokkos::SCZM::normalStiffness(
            _tangent_neighbor(neighbor, neighbor_qp), normal, _ndisp);
      }
      const auto parameters = Moose::Kokkos::SCZM::automaticStabilization(_settings.gamma,
                                                                          face_area,
                                                                          primary_stiffness,
                                                                          primary_volume_measure,
                                                                          neighbor_stiffness,
                                                                          neighbor_volume_measure);
      _stabilization_beta(index) = parameters.beta;
      _primary_weight(index) = parameters.primary_weight;
    }

    const Real primary_weight = _primary_weight(index);
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
        average_traction(i) +=
            (primary_weight * primary_stress + (1.0 - primary_weight) * neighbor_stress) *
            normal(J);
      }
    _bulk_traction[index] = average_traction;
  }
}

/// Device functor solving the generalized Robin problem on one interface face with law TSL
template <typename TSL>
struct SCZMRobinPrepass
{
  KokkosSCZMStabilizationData uo;
  TSL tsl;

  KOKKOS_FUNCTION void operator()(const Moose::Kokkos::ThreadID tid) const
  {
    uo.robinFace(tsl, tid);
  }
};

template <typename TSL>
void
KokkosSCZMStabilizationData::robinPrepass(const TSL & tsl) const
{
  // The functor holds copies, so every launch sees the current host state of both objects
  ::Kokkos::parallel_for(Moose::Kokkos::Policy(0, static_cast<Moose::Kokkos::ThreadID>(numFaces())),
                         SCZMRobinPrepass<TSL>{*this, tsl});
  ::Kokkos::fence();
}

template <typename TSL>
KOKKOS_FUNCTION void
KokkosSCZMStabilizationData::robinFace(const TSL & tsl, const Moose::Kokkos::ThreadID tid) const
{
  const auto & face = kokkosInterfaceFace(tid);
  auto primary = makePrimaryInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());

  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
  {
    const auto index = face.qp_map_offset + qp;
    const auto normal = _shifted ? _true_normals[index] : primary.normals(qp);
    const auto & average_traction = _bulk_traction[index];

    const auto local_law = tsl.trialLaw(primary, qp);
    Moose::Kokkos::SCZM::GlobalTractionTotalLagrangian<std::decay_t<decltype(local_law)>> evaluator;
    evaluator.local_law = local_law;
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
    Moose::Kokkos::Real3 jump{};
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      jump(i) = jump_property(i);
      evaluator.reference_normal(i) = normal(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
        evaluator.total_rotation(i, j) = total_rotation(i, j);
    }
    // Small strain keeps F = I and the identity rotations, so the area ratio is one and the
    // evaluator reduces to the small-strain traction Q0 t(Q0^T j)
    if (_finite_strain)
    {
      const auto reference_rotation = _czm_reference_rotation(primary, qp);
      const auto rotation = _czm_rotation(primary, qp);
      const auto deformation_gradient = _F_czm(primary, qp);
      for (unsigned int i = 0; i < _ndisp; ++i)
        for (unsigned int j = 0; j < _ndisp; ++j)
        {
          evaluator.deformation_gradient(i, j) = deformation_gradient(i, j);
          evaluator.rotation(i, j) = rotation(i, j);
          evaluator.reference_rotation(i, j) = reference_rotation(i, j);
        }
    }
    if (_ndisp < 3)
      evaluator.reference_normal(2) = 0.0;

    const auto stabilized =
        Moose::Kokkos::SCZM::GeneralizedRobinSolver::solve(evaluator,
                                                           jump,
                                                           average_traction,
                                                           _stabilization_beta(index),
                                                           _ndisp,
                                                           local_law.isAffine());
    if (!stabilized.converged)
    {
      ::Kokkos::atomic_exchange(&_prepass_failure[0], 1);
      continue;
    }
    _stabilized_traction[index] = stabilized.traction;
    _auxiliary_jump[index] = stabilized.auxiliary_jump;
    // A stateful law acts at the auxiliary jump, so it records its state there
    if constexpr (Moose::Kokkos::SCZM::RecordsTrialState<TSL, decltype(primary)>::value)
      tsl.recordTrialState(primary, qp, evaluator.toLocalJump(stabilized.auxiliary_jump, _ndisp));
    if (_need_tangents)
    {
      _robin_W[index] = stabilized.dtraction_djump;
      _inverse_robin_tangent[index] = stabilized.inverse_robin_tangent;
      if (!_finite_strain)
        continue;
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
