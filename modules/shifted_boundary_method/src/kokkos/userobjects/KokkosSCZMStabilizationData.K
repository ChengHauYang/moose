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
#include "MaterialBase.h"
#include "SCZMNEML2TrialTractionProvider.h"

#ifdef NEML2_ENABLED
#include "MooseException.h"
#include "NEML2BatchIndexGenerator.h"
#include "NEML2ModelExecutor.h"
#include "NEML2ToKokkosMaterialProperty.h"
#endif

#include "libmesh/fe_map.h"

#include <algorithm>
#include <cmath>

registerKokkosInterfaceUserObject("ShiftedBoundaryMethodApp", KokkosSCZMStabilizationData);

InputParameters
KokkosSCZMStabilizationData::validParams()
{
  auto params = Moose::Kokkos::InterfaceUserObject::validParams();
  params.addRequiredCoupledVar("displacements", "The displacement variables.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addParam<MooseEnum>("strain",
                             MooseEnum("SMALL FINITE", "FINITE"),
                             "Kinematics of the cohesive zone. SMALL uses the fixed reference "
                             "rotation and no area change.");
  params.addParam<bool>("no_shifted", false, "Disable shifted geometry.");
  params.addParam<UserObjectName>("sbm_distance_uo", "Surrogate-interface mapping provider.");
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
    _finite_strain(getParam<MooseEnum>("strain") == "FINITE"),
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
    _stabilization_beta(declareRestartableData<Moose::Kokkos::Array<Real>>("stabilization_beta")),
    _beta_initialized(declareRestartableData<bool>("beta_initialized", false))
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
  if (_element_linear_recovery && !_finite_strain)
    paramError("stress_recovery", "'element_linear' requires finite strain.");
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
  if (_finite_strain)
  {
    _czm_reference_rotation =
        getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_reference_rotation");
    _czm_rotation = getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_rotation");
    _F_czm = getKokkosMaterialPropertyByName<Real, 2>(prefix + "F_czm");
  }

  if (_sbm_distance_uo)
    _fe_problem.addKokkosMeshInitializationHook(
        std::bind(&KokkosSCZMStabilizationData::initializeSurrogateInterfaceMapping, this));
}

KokkosSCZMStabilizationData::KokkosSCZMStabilizationData(const KokkosSCZMStabilizationData & object)
  : Moose::Kokkos::InterfaceUserObject(object),
    _ndisp(object._ndisp),
    _finite_strain(object._finite_strain),
    _shifted(object._shifted),
    _element_linear_recovery(object._element_linear_recovery),
    _coupled_cubic_stabilization(object._coupled_cubic_stabilization),
    _stabilization_linear_stiffness(object._stabilization_linear_stiffness),
    _stabilization_cubic_stiffness(object._stabilization_cubic_stiffness),
    _settings(object._settings),
    _sbm_distance_uo(object._sbm_distance_uo),
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
    _stress(object._stress),
    _stress_neighbor(object._stress_neighbor),
    _volume_stress(object._volume_stress),
    _volume_tangent(object._volume_tangent),
    _trial_provider(object._trial_provider),
    _mapping_ready(object._mapping_ready),
    _recovery_initialized(object._recovery_initialized),
    _beta_initialized(object._beta_initialized),
    _bulk_and_robin_valid(object._bulk_and_robin_valid),
    _robin_tangents_valid(object._robin_tangents_valid),
    _need_tangents(object._need_tangents),
    _prepass_failure(object._prepass_failure)
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
  findTrialTractionProvider();
}

void
KokkosSCZMStabilizationData::findTrialTractionProvider()
{
  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  const auto traction_name = prefix + "interface_traction";
  const MaterialBase * supplier = nullptr;
  for (const auto & material : _fe_problem.getKokkosMaterialsWarehouse().getActiveObjects())
  {
    if (!material->getSuppliedItems().count(traction_name))
      continue;
    const auto & material_boundaries = material->boundaryIDs();
    if (std::none_of(boundaryIDs().begin(),
                     boundaryIDs().end(),
                     [&](const auto id) { return material_boundaries.count(id); }))
      continue;
    if (supplier && supplier != material.get())
      mooseError("Weighted traction stabilization on '",
                 name(),
                 "' found two materials supplying '",
                 traction_name,
                 "' on its interface: '",
                 supplier->name(),
                 "' and '",
                 material->name(),
                 "'.");
    supplier = material.get();
  }
  if (!supplier)
    mooseError("Weighted traction stabilization on '",
               name(),
               "' found no Kokkos material supplying '",
               traction_name,
               "' on its interface.");
#ifdef NEML2_ENABLED
  // A NEML2 cohesive model is evaluated at trial jumps by replacing its local-jump input
  if (const auto * const neml2_supplier =
          dynamic_cast<const NEML2ToKokkosSideVectorMaterialProperty *>(supplier))
  {
    if (neml2_supplier->isParamValid("neml2_input_derivative") ||
        neml2_supplier->isParamValid("neml2_parameter_derivative"))
      mooseError("Weighted traction stabilization on '",
                 name(),
                 "' requires '",
                 traction_name,
                 "' to be a NEML2 output, but '",
                 supplier->name(),
                 "' retrieves a derivative.");
    const auto & executor = neml2_supplier->neml2Executor();
    const auto jump_name = prefix + "interface_displacement_jump";
    std::string jump_input;
    try
    {
      jump_input = executor.inputForMaterialProperty(jump_name);
    }
    catch (const MooseException & e)
    {
      mooseError(e.what());
    }
    if (jump_input.empty())
      mooseError("Weighted traction stabilization on '",
                 name(),
                 "' requires the NEML2 model of '",
                 executor.name(),
                 "' to gather its cohesive jump from the material property '",
                 jump_name,
                 "'.");
    const auto & generator = _fe_problem.getUserObject<NEML2BatchIndexGenerator>(
        executor.getParam<UserObjectName>("batch_index_generator"));
    // The provider publishes the evaluation at the auxiliary jump of a model with history
    auto & mutable_executor = _fe_problem.getUserObject<NEML2ModelExecutor>(executor.name());
    _owned_trial_provider = std::make_unique<SCZMNEML2TrialTractionProvider>(
        mutable_executor, generator, jump_input, neml2_supplier->neml2OutputName());
    _trial_provider = _owned_trial_provider.get();
  }
  else
#endif
    _trial_provider = dynamic_cast<const SCZMTrialTractionProvider *>(supplier);
  if (!_trial_provider)
    mooseError("Weighted traction stabilization requires the traction-separation material '",
               supplier->name(),
               "' (",
               supplier->type(),
               ") to evaluate its cohesive law at trial jumps, which it does not support.");
  _trial_provider->checkStabilizationLaw(*this);
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
  if (_finite_strain)
    _robin_weighted_dF.create(n_qps, 27);
  _prepass_failure.create(1);
  if (!_settings.beta_override)
  {
    if (_stabilization_beta->size() == 0)
      _stabilization_beta->create(n_qps);
    else if (_stabilization_beta->size() != n_qps)
      mooseError("Recovered SCZM stabilization_beta size does not match the interface QP layout.");
    _stabilization_beta->copyToDevice();
  }
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
    dispatch();
    _stabilization_beta->copyToHost();
    for (const auto i : index_range(*_stabilization_beta))
      if (!(_stabilization_beta(i) > 0.0) || !std::isfinite(_stabilization_beta(i)))
        mooseError(
            "SCZM automatic stabilization_beta must be finite and positive; check the normal "
            "directional bulk tangents and adjacent element volumes.");
    _beta_initialized = true;
  }

  _prepass_failure[0] = 0;
  _prepass_failure.copyToDevice();
  dispatch();
  _trial_provider->robinPrepass(*this);
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

/// Device functor gathering the generalized Robin inputs on one interface face
struct SCZMGatherRobinInputs
{
  KokkosSCZMStabilizationData uo;
  KokkosSCZMStabilizationData::RobinInputs inputs;

  KOKKOS_FUNCTION void operator()(const Moose::Kokkos::ThreadID tid) const
  {
    uo.gatherRobinInputsFace(inputs, tid);
  }
};

/// Device functor storing the host generalized Robin result of one interface QP
struct SCZMStoreRobinResults
{
  KokkosSCZMStabilizationData uo;
  KokkosSCZMStabilizationData::RobinResults results;

  KOKKOS_FUNCTION void operator()(const Moose::Kokkos::ThreadID index) const
  {
    uo.storeRobinResult(results, index);
  }
};

void
KokkosSCZMStabilizationData::gatherRobinInputs(RobinInputs & inputs) const
{
  const auto n_qps = numQps();
  if (inputs.shifted_jump.size() != n_qps)
  {
    inputs.shifted_jump.create(n_qps);
    inputs.bulk_traction.create(n_qps);
    inputs.reference_normal.create(n_qps);
    inputs.beta.create(n_qps);
    inputs.total_rotation.create(n_qps);
    inputs.reference_rotation.create(n_qps);
    inputs.rotation.create(n_qps);
    inputs.deformation_gradient.create(n_qps);
  }

  ::Kokkos::parallel_for(Moose::Kokkos::Policy(0, static_cast<Moose::Kokkos::ThreadID>(numFaces())),
                         SCZMGatherRobinInputs{*this, inputs});
  ::Kokkos::fence();

  inputs.shifted_jump.copyToHost();
  inputs.bulk_traction.copyToHost();
  inputs.reference_normal.copyToHost();
  inputs.beta.copyToHost();
  inputs.total_rotation.copyToHost();
  inputs.reference_rotation.copyToHost();
  inputs.rotation.copyToHost();
  inputs.deformation_gradient.copyToHost();
}

KOKKOS_FUNCTION void
KokkosSCZMStabilizationData::gatherRobinInputsFace(const RobinInputs & inputs,
                                                   const Moose::Kokkos::ThreadID tid) const
{
  const auto & face = kokkosInterfaceFace(tid);
  auto primary =
      Moose::Kokkos::makePrimaryInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());

  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
  {
    const auto index = face.qp_map_offset + qp;
    const auto normal = _shifted ? _true_normals[index] : primary.normals(qp);
    const auto jump_property = _displacement_jump_global(primary, qp);
    const auto total_rotation_property = _czm_total_rotation(primary, qp);

    Moose::Kokkos::Real3 jump{};
    Moose::Kokkos::Real3 reference_normal{};
    Moose::Kokkos::Real33 total_rotation{};
    Moose::Kokkos::Real33 reference_rotation{};
    Moose::Kokkos::Real33 rotation{};
    Moose::Kokkos::Real33 deformation_gradient{};
    for (unsigned int i = 0; i < 3; ++i)
    {
      total_rotation(i, i) = 1.0;
      reference_rotation(i, i) = 1.0;
      rotation(i, i) = 1.0;
      deformation_gradient(i, i) = 1.0;
    }
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      jump(i) = jump_property(i);
      reference_normal(i) = normal(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
        total_rotation(i, j) = total_rotation_property(i, j);
    }
    // Small strain keeps F = I and the identity rotations, as in robinFace()
    if (_finite_strain)
    {
      const auto reference_rotation_property = _czm_reference_rotation(primary, qp);
      const auto rotation_property = _czm_rotation(primary, qp);
      const auto deformation_gradient_property = _F_czm(primary, qp);
      for (unsigned int i = 0; i < _ndisp; ++i)
        for (unsigned int j = 0; j < _ndisp; ++j)
        {
          reference_rotation(i, j) = reference_rotation_property(i, j);
          rotation(i, j) = rotation_property(i, j);
          deformation_gradient(i, j) = deformation_gradient_property(i, j);
        }
    }

    inputs.shifted_jump[index] = jump;
    inputs.bulk_traction[index] = _bulk_traction[index];
    inputs.reference_normal[index] = reference_normal;
    inputs.beta[index] = _settings.beta_override ? _settings.beta : _stabilization_beta(index);
    inputs.total_rotation[index] = total_rotation;
    inputs.reference_rotation[index] = reference_rotation;
    inputs.rotation[index] = rotation;
    inputs.deformation_gradient[index] = deformation_gradient;
  }
}

void
KokkosSCZMStabilizationData::storeRobinResults(const RobinResults & results) const
{
  ::Kokkos::parallel_for(Moose::Kokkos::Policy(0, static_cast<Moose::Kokkos::ThreadID>(numQps())),
                         SCZMStoreRobinResults{*this, results});
  ::Kokkos::fence();
}

KOKKOS_FUNCTION void
KokkosSCZMStabilizationData::storeRobinResult(const RobinResults & results,
                                              const dof_id_type index) const
{
  if (!results.converged[index])
  {
    ::Kokkos::atomic_exchange(&_prepass_failure[0], 1);
    return;
  }
  _stabilized_traction[index] = results.traction[index];
  _auxiliary_jump[index] = results.auxiliary_jump[index];
  if (!_need_tangents)
    return;
  _robin_W[index] = results.robin_W[index];
  _inverse_robin_tangent[index] = results.inverse_robin_tangent[index];
  // 27 = 3 x 3 x 3 entries (a, k, l) of the S-weighted d(traction_a)/d(F_kl)
  if (_finite_strain)
    for (unsigned int entry = 0; entry < 27; ++entry)
      _robin_weighted_dF(index, entry) = results.robin_weighted_dF(index, entry);
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
