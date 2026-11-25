//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosSCZMStabilizationData.h"

#include "BoundaryShortestDistanceToSurface.h"
#include "FEProblemBase.h"
#include "KokkosInterfaceDatum.h"

#include "libmesh/fe_map.h"

#include <cmath>

registerKokkosInterfaceUserObject("ShiftedBoundaryMethodApp", KokkosSCZMStabilizationData);

InputParameters
KokkosSCZMStabilizationData::validParams()
{
  auto params = Moose::Kokkos::InterfaceUserObject::validParams();
  params.addRequiredCoupledVar("displacements", "The displacement variables.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addParam<bool>("no_shifted", false, "Disable shifted geometry.");
  params.addParam<UserObjectName>("sbm_distance_uo", "Surrogate-interface mapping provider.");
  params.addParam<UserObjectName>("batch_index_generator",
                                  "Optional interface-only NEML2 batch generator used to enforce "
                                  "primary-side state ownership.");
  params.addParam<MooseEnum>("stress_recovery",
                             MooseEnum("none element_linear", "none"),
                             "Optional element-local affine recovery of adjacent PK1 stress.");
  params.addParam<MaterialPropertyName>("stress", "pk1_stress", "Interface PK1 stress.");
  params.addParam<MaterialPropertyName>(
      "tangent", "pk1_jacobian", "Interface derivative d(PK1)/d(F).");
  params.addParam<MaterialPropertyName>(
      "volume_stress", "pk1_stress", "Volume PK1 stress used by affine recovery.");
  params.addParam<MaterialPropertyName>(
      "volume_tangent", "dpk1_d_grad_u", "Complete volume derivative d(PK1)/d(grad(u)).");
  params.addParam<MooseEnum>("stabilization_cohesive_law",
                             MooseEnum("affine coupled_cubic", "affine"),
                             "Native cohesive evaluator used by the generalized Robin solve.");
  params.addParam<Real>("stabilization_linear_stiffness", "Coupled-cubic linear coefficient.");
  params.addParam<Real>("stabilization_cubic_stiffness", "Coupled-cubic cubic coefficient.");
  SCZMWeightedTractionStabilization::addParams(params);
  params.set<ExecFlagEnum>("execute_on", true) = {EXEC_NONE};
  params.addClassDescription(
      "Produces the shared Kokkos prepass data for weighted-traction shifted cohesive zones.");
  return params;
}

KokkosSCZMStabilizationData::KokkosSCZMStabilizationData(const InputParameters & parameters)
  : Moose::Kokkos::InterfaceUserObject(parameters),
    _ndisp(coupledComponents("displacements")),
    _shifted(!getParam<bool>("no_shifted")),
    _element_linear_recovery(getParam<MooseEnum>("stress_recovery") == "element_linear"),
    _coupled_cubic_stabilization(getParam<MooseEnum>("stabilization_cohesive_law") ==
                                 "coupled_cubic"),
    _stabilization_linear_stiffness(isParamValid("stabilization_linear_stiffness")
                                        ? getParam<Real>("stabilization_linear_stiffness")
                                        : 0.0),
    _stabilization_cubic_stiffness(isParamValid("stabilization_cubic_stiffness")
                                       ? getParam<Real>("stabilization_cubic_stiffness")
                                       : 0.0),
    _settings(SCZMWeightedTractionStabilization::settings(*this)),
    _sbm_distance_uo(isParamValid("sbm_distance_uo")
                         ? &getUserObject<BoundaryShortestDistanceToSurface>("sbm_distance_uo")
                         : nullptr),
    _batch_index_generator(isParamValid("batch_index_generator")
                               ? &getUserObject<NEML2BatchIndexGenerator>("batch_index_generator")
                               : nullptr)
{
  if (!_settings.enabled)
    paramError("stabilization",
               "The shared stabilization producer requires weighted stabilization.");
  if (_ndisp != _fe_problem.mesh().dimension() || _ndisp < 1 || _ndisp > 3)
    paramError("displacements", "One, two, or three displacements matching the mesh are required.");
  if (_shifted && !_sbm_distance_uo)
    paramError("sbm_distance_uo", "This parameter is required unless 'no_shifted = true'.");
  if (_element_linear_recovery && !_shifted)
    paramError("stress_recovery", "'element_linear' requires the true-interface mapping.");
  if (_coupled_cubic_stabilization && !isParamValid("stabilization_linear_stiffness"))
    paramError("stabilization_linear_stiffness",
               "This parameter is required for the coupled_cubic evaluator.");
  if (_coupled_cubic_stabilization && !isParamValid("stabilization_cubic_stiffness"))
    paramError("stabilization_cubic_stiffness",
               "This parameter is required for the coupled_cubic evaluator.");
  if (_coupled_cubic_stabilization &&
      (_stabilization_linear_stiffness < 0.0 || _stabilization_cubic_stiffness < 0.0))
    paramError("stabilization_cohesive_law", "The coupled_cubic coefficients must be nonnegative.");

  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  if (_element_linear_recovery || !_settings.beta_override)
  {
    auto & volume_data = _fe_problem.getKokkosMaterialData(Moose::BLOCK_MATERIAL_DATA, this);
    const auto volume_tangent_name = prefix + getParam<MaterialPropertyName>("volume_tangent");
    _volume_tangent = getKokkosMaterialPropertyByName<Real, 4>(volume_tangent_name, volume_data);
    _fe_problem.getKokkosMaterialPropertyStorage().requestGhostedProperty(volume_tangent_name);

    if (_element_linear_recovery)
    {
      const auto volume_stress_name = prefix + getParam<MaterialPropertyName>("volume_stress");
      _volume_stress = getKokkosMaterialPropertyByName<Real, 2>(volume_stress_name, volume_data);
      _fe_problem.getKokkosMaterialPropertyStorage().requestGhostedProperty(volume_stress_name);
    }
  }

  if (!_element_linear_recovery)
  {
    const auto stress_name = prefix + getParam<MaterialPropertyName>("stress");
    _stress = getKokkosMaterialPropertyByName<Real, 2>(stress_name);
    _stress_neighbor = getKokkosNeighborMaterialPropertyByName<Real, 2>(stress_name);
  }

  _displacement_jump_global =
      getKokkosMaterialPropertyByName<Real, 1>(prefix + "displacement_jump_global");
  _czm_total_rotation = getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_total_rotation");
  _czm_reference_rotation =
      getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_reference_rotation");
  _czm_rotation = getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_rotation");
  _F_czm = getKokkosMaterialPropertyByName<Real, 2>(prefix + "F_czm");
  _interface_traction = getKokkosMaterialPropertyByName<Real, 1>(prefix + "interface_traction");
  _dinterface_traction_djump =
      getKokkosMaterialPropertyByName<Real, 2>(prefix + "dinterface_traction_djump");

  if (_sbm_distance_uo)
    _fe_problem.addKokkosMeshInitializationHook(
        std::bind(&KokkosSCZMStabilizationData::initializeSurrogateInterfaceMapping, this));
}

KokkosSCZMStabilizationData::KokkosSCZMStabilizationData(const KokkosSCZMStabilizationData & object)
  : Moose::Kokkos::InterfaceUserObject(object),
    _ndisp(object._ndisp),
    _shifted(object._shifted),
    _element_linear_recovery(object._element_linear_recovery),
    _coupled_cubic_stabilization(object._coupled_cubic_stabilization),
    _stabilization_linear_stiffness(object._stabilization_linear_stiffness),
    _stabilization_cubic_stiffness(object._stabilization_cubic_stiffness),
    _settings(object._settings),
    _sbm_distance_uo(object._sbm_distance_uo),
    _batch_index_generator(object._batch_index_generator),
    _true_normals(object._true_normals),
    _surrogate_distance_vectors(object._surrogate_distance_vectors),
    _face_qp_offsets(object._face_qp_offsets),
    _bulk_traction(object._bulk_traction),
    _stabilization_beta(object._stabilization_beta),
    _stabilized_traction(object._stabilized_traction),
    _auxiliary_jump(object._auxiliary_jump),
    _robin_W(object._robin_W),
    _inverse_robin_tangent(object._inverse_robin_tangent),
    _robin_weighted_dF(object._robin_weighted_dF),
    _recovery_offsets(object._recovery_offsets),
    _recovery_weights(object._recovery_weights),
    _displacement_jump_global(object._displacement_jump_global),
    _czm_total_rotation(object._czm_total_rotation),
    _czm_reference_rotation(object._czm_reference_rotation),
    _czm_rotation(object._czm_rotation),
    _F_czm(object._F_czm),
    _interface_traction(object._interface_traction),
    _dinterface_traction_djump(object._dinterface_traction_djump),
    _stress(object._stress),
    _stress_neighbor(object._stress_neighbor),
    _volume_stress(object._volume_stress),
    _volume_tangent(object._volume_tangent),
    _mapping_ready(object._mapping_ready),
    _recovery_initialized(object._recovery_initialized),
    _beta_initialized(object._beta_initialized),
    _bulk_and_robin_valid(object._bulk_and_robin_valid),
    _robin_tangents_valid(object._robin_tangents_valid),
    _need_tangents(object._need_tangents),
    _initialize_beta_only(object._initialize_beta_only),
    _prepass_failure(object._prepass_failure),
    _validated_map_generation(object._validated_map_generation)
{
}

void
KokkosSCZMStabilizationData::initializeSurrogateInterfaceMapping()
{
  const auto & mapping = _sbm_distance_uo->registerKokkosSurrogateInterfaceMapping(
      boundaryIDs(), kokkosInterfaceFaces());
  _true_normals = mapping.true_normals;
  _surrogate_distance_vectors = mapping.surrogate_distance_vectors;
  _mapping_ready = true;
}

void
KokkosSCZMStabilizationData::initialSetup()
{
  Moose::Kokkos::InterfaceUserObject::initialSetup();
  allocateArrays();
  validateCanonicalBatchOwnership();
}

void
KokkosSCZMStabilizationData::allocateArrays()
{
  dof_id_type n_qps = 0;
  _face_qp_offsets.create(_fe_problem.mesh().getMaxSidesPerElem(),
                          kokkosMesh().getNumLocalAndPossiblyOneNeighborLayerGhostElements());
  _face_qp_offsets = libMesh::DofObject::invalid_id;
  for (const auto & face : kokkosInterfaceFaces())
  {
    n_qps = std::max(n_qps, face.qp_map_offset + face.n_qps);
    _face_qp_offsets(face.side, face.elem) = face.qp_map_offset;
  }
  _face_qp_offsets.copyToDevice();
  _bulk_traction.create(n_qps);
  _stabilized_traction.create(n_qps);
  _auxiliary_jump.create(n_qps);
  _robin_W.create(n_qps);
  _inverse_robin_tangent.create(n_qps);
  _robin_weighted_dF.create(n_qps, 27);
  _prepass_failure.create(1);
  if (!_settings.beta_override)
    _stabilization_beta.create(n_qps);
}

void
KokkosSCZMStabilizationData::invalidateAssemblyCache()
{
  _bulk_and_robin_valid = false;
  _robin_tangents_valid = false;
}

void
KokkosSCZMStabilizationData::ensurePrepared(const bool need_tangents)
{
  // Recovery targets the true-interface point, whose distance vectors the distance user object
  // fills only on EXEC_INITIAL, after initialSetup()
  initializeRecovery();

  if (_bulk_and_robin_valid && (!need_tangents || _robin_tangents_valid))
    return;

  _need_tangents = need_tangents;
  const auto dispatch = [this]()
  {
    _prepass_dispatcher.reset();
    _prepass_dispatcher =
        Moose::Kokkos::DispatcherRegistry::build<Moose::Kokkos::InterfaceUserObject::InterfaceLoop>(
            this, type());
    _prepass_dispatcher->parallelFor(
        Moose::Kokkos::Policy(0, static_cast<Moose::Kokkos::ThreadID>(numFaces())));
    ::Kokkos::fence();
  };

  if (!_settings.beta_override && !_beta_initialized)
  {
    _initialize_beta_only = true;
    dispatch();
    _stabilization_beta.copyToHost();
    for (const auto i : index_range(_stabilization_beta))
      if (!(_stabilization_beta[i] > 0.0) || !std::isfinite(_stabilization_beta[i]))
        mooseError(
            "SCZM automatic stabilization_beta must be finite and positive; check the normal "
            "directional bulk tangents and adjacent element volumes.");
    _beta_initialized = true;
  }

  _initialize_beta_only = false;
  _prepass_failure[0] = 0;
  _prepass_failure.copyToDevice();
  dispatch();
  _prepass_failure.copyToHost();
  if (_prepass_failure[0])
  {
    invalidateAssemblyCache();
    _fe_problem.setException("Generalized SCZM Robin solve failed.");
    return;
  }

  _bulk_and_robin_valid = true;
  _robin_tangents_valid = need_tangents;
}

void
KokkosSCZMStabilizationData::validateCanonicalBatchOwnership()
{
  if (!_batch_index_generator || _batch_index_generator->mapGeneration() == 0 ||
      _validated_map_generation == _batch_index_generator->mapGeneration())
    return;

  auto primary = _batch_index_generator->kokkosFacePrimaryBatchIndices();
  auto neighbor = _batch_index_generator->kokkosFaceNeighborBatchIndices();
  primary.copyToHost();
  neighbor.copyToHost();
  const auto invalid = NEML2BatchIndexGenerator::invalidKokkosBatchIndex();
  for (const auto & face : kokkosInterfaceFaces())
  {
    if (primary[face.face_index] == invalid)
      mooseError("NEML2 SCZM requires a primary-side batch row for every cohesive face.");
    if (neighbor[face.face_index] != invalid)
      mooseError("NEML2 SCZM cohesive state must be owned only by the canonical primary side; the "
                 "batch generator includes both sides of one cohesive face.");
  }
  _validated_map_generation = _batch_index_generator->mapGeneration();
}

void
KokkosSCZMStabilizationData::initializeRecovery()
{
  if (!_element_linear_recovery || _recovery_initialized)
    return;

  dof_id_type n_qps = 0;
  dof_id_type n_weights = 0;
  Real maximum_amplification = 0.0;
  for (const auto & face : kokkosInterfaceFaces())
  {
    n_qps = std::max(n_qps, face.qp_map_offset + face.n_qps);
    auto primary =
        Moose::Kokkos::makePrimaryInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());
    auto neighbor =
        Moose::Kokkos::makeNeighborInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());
    Datum primary_volume(
        face.elem, libMesh::invalid_uint, face.neighbor, kokkosAssembly(), kokkosSystems(), true);
    Datum neighbor_volume(
        face.neighbor, libMesh::invalid_uint, face.elem, kokkosAssembly(), kokkosSystems(), true);
    n_weights += 2 * face.n_qps * (primary_volume.n_qps() + neighbor_volume.n_qps());
  }
  _recovery_offsets.create(4 * n_qps);
  _recovery_weights.create(n_weights);

  dof_id_type next_weight = 0;
  for (const auto & face : kokkosInterfaceFaces())
  {
    auto primary =
        Moose::Kokkos::makePrimaryInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());
    Datum primary_volume(
        face.elem, libMesh::invalid_uint, face.neighbor, kokkosAssembly(), kokkosSystems(), true);
    Datum neighbor_volume(
        face.neighbor, libMesh::invalid_uint, face.elem, kokkosAssembly(), kokkosSystems(), true);

    for (const auto qp : make_range(face.n_qps))
    {
      const auto map_index = face.qp_map_offset + qp;
      const auto surrogate_point = primary.q_point(qp);
      const auto distance = _surrogate_distance_vectors[map_index];
      const Point true_point(surrogate_point(0) + distance(0),
                             surrogate_point(1) + distance(1),
                             surrogate_point(2) + distance(2));
      for (const auto component : make_range(_fe_problem.mesh().dimension()))
        if (!std::isfinite(true_point(component)))
          paramError("stress_recovery",
                     "The true-interface point for element ",
                     face.elem_id,
                     " is non-finite.");

      for (const auto target_index : make_range(2))
      {
        const Point target_point =
            target_index == 0 ? true_point
                              : Point(surrogate_point(0), surrogate_point(1), surrogate_point(2));
        for (const auto side : make_range(2))
        {
          auto & volume = side == 0 ? primary_volume : neighbor_volume;
          const auto * const elem =
              _fe_problem.mesh().elemPtr(side == 0 ? face.elem_id : face.neighbor_id);
          const auto n_volume_qps = volume.n_qps();
          _recovery_offsets[4 * map_index + 2 * target_index + side] = next_weight;

          RealEigenVector weights(n_volume_qps);
          std::vector<Point> reproduction_points(n_volume_qps);
          const bool analytical_hex8 = elem->type() == HEX8 && n_volume_qps == 8;
          Point reproduction_target = target_point;
          if (analytical_hex8)
          {
            const Point target = libMesh::FEMap::inverse_map(elem->dim(), elem, target_point);
            reproduction_target = target;
            for (const auto component : make_range(elem->dim()))
              if (!std::isfinite(target(component)))
                paramError("stress_recovery",
                           "The inverse-mapped true-interface point for element ",
                           elem->id(),
                           " is non-finite.");
            for (const auto volume_qp : make_range(n_volume_qps))
            {
              const auto x = volume.q_point(volume_qp);
              for (const auto component : make_range(elem->dim()))
                if (!std::isfinite(x(component)))
                  paramError("stress_recovery",
                             "Element ",
                             elem->id(),
                             " has a non-finite volume quadrature point.");
              const Point physical_point(x(0), x(1), x(2));
              const Point sample = libMesh::FEMap::inverse_map(elem->dim(), elem, physical_point);
              reproduction_points[volume_qp] = sample;
              for (const auto component : make_range(elem->dim()))
                if (!std::isfinite(sample(component)))
                  paramError("stress_recovery",
                             "A volume quadrature point inverse map for element ",
                             elem->id(),
                             " is non-finite.");
              weights(volume_qp) = (1.0 + 3.0 * (target(0) * sample(0) + target(1) * sample(1) +
                                                 target(2) * sample(2))) /
                                   8.0;
            }
          }
          else
          {
            const auto basis_size = elem->dim() + 1;
            if (n_volume_qps < basis_size)
              paramError("stress_recovery",
                         "Element ",
                         elem->id(),
                         " has ",
                         n_volume_qps,
                         " volume quadrature points, fewer than the ",
                         basis_size,
                         " samples required for affine stress recovery.");

            RealEigenMatrix design(n_volume_qps, basis_size);
            for (const auto volume_qp : make_range(n_volume_qps))
            {
              const auto point = volume.q_point(volume_qp);
              reproduction_points[volume_qp] = Point(point(0), point(1), point(2));
              design(volume_qp, 0) = 1.0;
              for (const auto component : make_range(elem->dim()))
              {
                if (!std::isfinite(point(component)))
                  paramError("stress_recovery",
                             "Element ",
                             elem->id(),
                             " has a non-finite volume quadrature point.");
                design(volume_qp, component + 1) = point(component) - target_point(component);
              }
            }

            const auto decomposition = design.transpose().completeOrthogonalDecomposition();
            if (decomposition.rank() != basis_size)
              paramError("stress_recovery",
                         "Volume quadrature points on element ",
                         elem->id(),
                         " are rank deficient for affine stress recovery.");
            RealEigenVector target = RealEigenVector::Zero(basis_size);
            target(0) = 1.0;
            weights = decomposition.solve(target);
          }

          Real coordinate_scale = std::max(1.0, reproduction_target.norm());
          for (const auto volume_qp : make_range(n_volume_qps))
            coordinate_scale = std::max(coordinate_scale, reproduction_points[volume_qp].norm());
          // This is the same scale as libMesh's geometric tolerance, enlarged to cover the
          // least-squares solve and inverse maps used to construct the weights.
          const Real constant_tolerance = 100.0 * libMesh::TOLERANCE;
          const Real affine_tolerance = constant_tolerance * coordinate_scale;
          Real constant = 0.0;
          Point affine;
          Real amplification = 0.0;
          for (const auto volume_qp : make_range(n_volume_qps))
          {
            const auto & point = reproduction_points[volume_qp];
            const Real weight = weights(volume_qp);
            if (!std::isfinite(weight))
              paramError("stress_recovery",
                         "Affine recovery produced a non-finite weight on element ",
                         elem->id(),
                         ".");
            constant += weight;
            amplification += std::abs(weight);
            for (const auto component : make_range(elem->dim()))
              affine(component) += weight * point(component);
            _recovery_weights[next_weight++] = weight;
          }

          if (std::abs(constant - 1.0) > constant_tolerance)
            paramError("stress_recovery",
                       "Affine recovery failed constant reproduction on element ",
                       elem->id(),
                       ".");
          for (const auto component : make_range(elem->dim()))
            if (std::abs(affine(component) - reproduction_target(component)) > affine_tolerance)
              paramError("stress_recovery",
                         "Affine recovery failed linear reproduction on element ",
                         elem->id(),
                         ".");
          if (!std::isfinite(amplification))
            paramError("stress_recovery",
                       "Affine recovery has non-finite weight amplification on element ",
                       elem->id(),
                       ".");
          maximum_amplification = std::max(maximum_amplification, amplification);
        }
      }
    }
  }

  _fe_problem.mesh().comm().max(maximum_amplification);
  if (_fe_problem.mesh().comm().rank() == 0)
    mooseInfoRepeated(
        "Maximum element-linear stress recovery weight amplification sum(abs(w_q)) = ",
        maximum_amplification);
  _recovery_offsets.copyToDevice();
  _recovery_weights.copyToDevice();
  _recovery_initialized = true;
}
