//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ShiftedCohesiveZoneAction.h"
#include "ActionWarehouse.h"
#include "AddMaterialAction.h"
#include "Factory.h"
#include "FEProblem.h"
#include "Conversion.h"
#include "NEML2Action.h"
#include "QuasiStaticSolidMechanicsPhysicsBase.h"

#include <algorithm>
#include <set>

namespace
{
bool
getInheritedVolumetricLockingCorrection(ActionWarehouse & awh)
{
  const auto quasi_static_physics = awh.getActions<QuasiStaticSolidMechanicsPhysicsBase>();
  for (const auto * const physics : quasi_static_physics)
    if (physics->isParamSetByUser("volumetric_locking_correction") &&
        physics->getParam<bool>("volumetric_locking_correction"))
      return true;

  return false;
}

bool
hasInheritedVolumetricLockingCorrectionSetting(ActionWarehouse & awh)
{
  const auto quasi_static_physics = awh.getActions<QuasiStaticSolidMechanicsPhysicsBase>();
  for (const auto * const physics : quasi_static_physics)
    if (physics->isParamSetByUser("volumetric_locking_correction"))
      return true;

  return false;
}

bool
hasOverlap(const std::vector<BoundaryName> & lhs, const std::vector<BoundaryName> & rhs)
{
  for (const auto & left : lhs)
    if (std::find(rhs.begin(), rhs.end(), left) != rhs.end())
      return true;

  return false;
}

/**
 * Map a stress or tangent name consumed by SCZM to the property name declared in [NEML2].
 *
 * SCZMInterfaceKernelSmallStrain accepts either a Cauchy tangent ("Jacobian_mult",
 * d(sigma)/d(strain), contracted with the symmetric displacement gradient) or a PK1 tangent
 * ("pk1_jacobian", d(PK1)/d(F), contracted with the full displacement gradient). When
 * ComputeLagrangianCauchyCustomStress wraps a NEML2 Cauchy stress and tangent into PK1
 * properties, return the wrapped Cauchy names; otherwise return property_name unchanged.
 */
MaterialPropertyName
resolveTransferredProperty(ActionWarehouse & awh, const MaterialPropertyName & property_name)
{
  for (const auto * const material_action : awh.getActions<AddMaterialAction>())
  {
    if (material_action->getMooseObjectType() != "ComputeLagrangianCauchyCustomStress")
      continue;

    const auto & params = material_action->getObjectParams();
    if (property_name == "pk1_stress" || property_name == "stress")
      return params.get<MaterialPropertyName>("custom_cauchy_stress");

    if (property_name == "pk1_jacobian" || property_name == "Jacobian_mult")
      return params.get<MaterialPropertyName>("custom_cauchy_jacobian");
  }

  return property_name;
}
}

// Register the action for various tasks in the MOOSE execution flow
registerMooseAction("ShiftedBoundaryMethodApp", ShiftedCohesiveZoneAction, "add_interface_kernel");
registerMooseAction("ShiftedBoundaryMethodApp", ShiftedCohesiveZoneAction, "add_material");
registerMooseAction("ShiftedBoundaryMethodApp", ShiftedCohesiveZoneAction, "add_function");
registerMooseAction("ShiftedBoundaryMethodApp", ShiftedCohesiveZoneAction, "add_user_object");
registerMooseAction("ShiftedBoundaryMethodApp",
                    ShiftedCohesiveZoneAction,
                    "add_master_action_material");
registerMooseAction("ShiftedBoundaryMethodApp", ShiftedCohesiveZoneAction, "add_aux_variable");
registerMooseAction("ShiftedBoundaryMethodApp", ShiftedCohesiveZoneAction, "add_aux_kernel");
registerMooseAction("ShiftedBoundaryMethodApp", ShiftedCohesiveZoneAction, "add_kernel");
registerMooseAction("ShiftedBoundaryMethodApp",
                    ShiftedCohesiveZoneAction,
                    "validate_coordinate_systems");

InputParameters
ShiftedCohesiveZoneAction::validParams()
{
  InputParameters params = CohesiveZoneAction::validParams();
  params.addClassDescription("Action to create Shifted Cohesive Zone Method (SCZM) objects "
                             "for non-interface-fitted meshes.");

  params.addParam<UserObjectName>(
      "sbm_distance_uo",
      "UserObject that provides signed distance and normal vector calculations.");
  params.addParam<bool>(
      "generate_sbm_distance",
      false,
      "Automatically create surface mesh builders, distance functions, and the boundary distance "
      "user object from the supplied boundaries.");
  params.addParam<std::vector<MeshGeneratorName>>(
      "surface_meshes",
      "Names of saved surface meshes corresponding one-to-one with boundary. If omitted, the "
      "boundary names are used.");
  params.addParam<bool>("check_surface_watertightness",
                        false,
                        "Whether generated surface mesh builders check mesh watertightness.");
  params.addParam<MeshGeneratorName>(
      "complete_interface_mesh",
      "Saved surface mesh containing the complete boundary of each subdomain. Setting this "
      "parameter automatically creates one interface manager and distance user object.");
  params.addParam<FileName>(
      "complete_interface_mesh_file",
      "File holding the surface mesh with the complete boundary of each subdomain, read directly "
      "instead of a saved mesh. Use this with a pre-split mesh (--use-split), for which the mesh "
      "generators that would save 'complete_interface_mesh' do not run. Mutually exclusive with "
      "'complete_interface_mesh' and 'complete_interface_meshes'.");
  params.addParam<std::vector<MeshGeneratorName>>(
      "complete_interface_meshes",
      "Saved watertight boundary mesh for each grain, one per grain, with the matching grain "
      "subdomain ID in 'interface_subdomain_ids'. The interface manager stitches them into the "
      "complete boundary mesh, equivalent to combining them upstream with SubdomainIDGenerator + "
      "CombinerGenerator. Mutually exclusive with 'complete_interface_mesh'.");
  params.addParam<std::vector<subdomain_id_type>>(
      "interface_subdomain_ids",
      "Grain subdomain ID for each mesh in 'complete_interface_meshes', in the same order.");
  params.addParam<std::vector<std::vector<SubdomainID>>>(
      "interface_subdomain_pairs",
      {},
      "Ordered subdomain pair for each boundary when it cannot be inferred from boundary names.");
  params.addParam<Real>("interface_matching_tolerance",
                        libMesh::TOLERANCE,
                        "Relative face-matching tolerance for the single-mesh interface manager.");

  params.addParam<bool>("no_shifted", false, "Disable shifted terms.");
  params.addParam<bool>("use_kokkos", false, "Create Kokkos SCZM objects.");
  params.addParam<bool>("directional_correction", true, "Add the directional correction terms.");
  params.addParam<MaterialPropertyName>(
      "stress", "stress", "Name of the stress tensor material property for small strain SCZM.");
  params.addParam<MaterialPropertyName>("tangent",
                                        "Jacobian_mult",
                                        "Name of the material Jacobian tensor property for small "
                                        "strain SCZM.");
  params.addParam<MooseEnum>(
      "tangent_definition",
      MooseEnum("auto stress_wrt_strain pk1_wrt_deformation_gradient pk2_wrt_deformation_gradient",
                "auto"),
      "Mathematical definition of the tangent material property. 'auto' recognizes the standard "
      "Jacobian_mult property as d(stress)/d(strain) and pk1_jacobian as d(PK1)/d(F). Select "
      "'stress_wrt_strain' or 'pk1_wrt_deformation_gradient' explicitly for a custom tangent "
      "property; select 'pk2_wrt_deformation_gradient' to convert PK2 input for Kokkos directional "
      "correction.");
  params.addParam<bool>(
      "volumetric_locking_correction",
      false,
      "Whether to apply volumetric locking correction to the directional correction Jacobian "
      "term in SCZM. If not specified, this value is automatically inherited from the solid "
      "mechanics physics block.");
  params.addParam<MaterialPropertyName>(
      "stabilized_tangent",
      "stabilized_pk1_jacobian",
      "Name of the derivative of the PK1 stress with respect to the F-bar deformation gradient at "
      "fixed physical deformation gradient, used by the total-Lagrangian volumetric locking "
      "correction.");

  return params;
}

ShiftedCohesiveZoneAction::ShiftedCohesiveZoneAction(const InputParameters & params)
  : CohesiveZoneAction(params), _use_kokkos(getParam<bool>("use_kokkos"))
{
  const bool generate_distance = getParam<bool>("generate_sbm_distance");
  const bool has_distance_uo = isParamSetByUser("sbm_distance_uo");
  const bool has_single_mesh = isParamSetByUser("complete_interface_mesh");
  const bool has_mesh_file = isParamSetByUser("complete_interface_mesh_file");
  const bool has_mesh_list = isParamSetByUser("complete_interface_meshes");
  // The single-mesh, mesh-file, and per-grain-list forms all drive the interface manager.
  const bool uses_manager = has_single_mesh || has_mesh_file || has_mesh_list;

  if (static_cast<unsigned int>(has_single_mesh) + static_cast<unsigned int>(has_mesh_file) +
          static_cast<unsigned int>(has_mesh_list) >
      1)
    paramError("complete_interface_meshes",
               "Specify only one of 'complete_interface_mesh', 'complete_interface_mesh_file', and "
               "'complete_interface_meshes'.");

  if (static_cast<unsigned int>(generate_distance) + static_cast<unsigned int>(has_distance_uo) +
          static_cast<unsigned int>(uses_manager) >
      1)
    paramError("sbm_distance_uo",
               "Specify only one of 'sbm_distance_uo', 'generate_sbm_distance', and "
               "'complete_interface_mesh'/'complete_interface_mesh_file'/"
               "'complete_interface_meshes'.");

  if (has_mesh_list)
  {
    if (!isParamSetByUser("interface_subdomain_ids"))
      paramError("interface_subdomain_ids",
                 "'interface_subdomain_ids' is required with 'complete_interface_meshes'.");
    if (getParam<std::vector<MeshGeneratorName>>("complete_interface_meshes").size() !=
        getParam<std::vector<subdomain_id_type>>("interface_subdomain_ids").size())
      paramError("interface_subdomain_ids",
                 "'interface_subdomain_ids' must have one entry per mesh in "
                 "'complete_interface_meshes'.");
  }
  else if (isParamSetByUser("interface_subdomain_ids"))
    paramError("interface_subdomain_ids",
               "This parameter is only valid with 'complete_interface_meshes'.");

  if (!generate_distance && isParamSetByUser("surface_meshes"))
    paramError("surface_meshes", "This parameter is only valid with 'generate_sbm_distance=true'.");
  if (!generate_distance && isParamSetByUser("check_surface_watertightness"))
    paramError("check_surface_watertightness",
               "This parameter is only valid with 'generate_sbm_distance=true'.");
  if (!uses_manager && isParamSetByUser("interface_matching_tolerance"))
    paramError("interface_matching_tolerance",
               "This parameter is only valid with 'complete_interface_mesh'/"
               "'complete_interface_mesh_file'/'complete_interface_meshes'.");

  if (generate_distance)
  {
    const auto surface_meshes = surfaceMeshNames();
    if (surface_meshes.size() != _boundary.size())
      paramError("surface_meshes", "The number of surface meshes must match the boundaries.");
    if (std::set<MeshGeneratorName>(surface_meshes.begin(), surface_meshes.end()).size() !=
        surface_meshes.size())
      paramError("surface_meshes", "Each saved surface mesh may only be used once.");
  }

  if (!uses_manager && isParamSetByUser("interface_subdomain_pairs"))
    paramError("interface_subdomain_pairs",
               "This parameter is only valid with 'complete_interface_mesh'/"
               "'complete_interface_mesh_file'/'complete_interface_meshes'.");
  if (uses_manager && isParamSetByUser("interface_subdomain_pairs") &&
      getParam<std::vector<std::vector<SubdomainID>>>("interface_subdomain_pairs").size() !=
          _boundary.size())
    paramError("interface_subdomain_pairs", "Provide exactly one subdomain pair per boundary.");

  if (_use_AD && isParamSetByUser("tangent"))
    paramError("tangent", "This parameter applies only to the non-AD SCZM interface kernel.");
  if (_use_AD && isParamSetByUser("tangent_definition"))
    paramError("tangent_definition",
               "This parameter applies only to the non-AD SCZM interface kernel. The AD kernel "
               "differentiates the residual automatically.");

  if (_use_kokkos && _use_AD)
    paramError("use_automatic_differentiation",
               "Kokkos SCZM currently supports only non-AD objects.");

  switch (_strain)
  {
    case Strain::Small:
    {
      _czm_kernel_name = _use_kokkos ? "KokkosSCZMInterfaceKernelSmallStrain"
                                     : (_use_AD ? "ADSCZMInterfaceKernelSmallStrain"
                                                : "SCZMInterfaceKernelSmallStrain");
      // For SCZM, we just need to evaulate the displacement jump correctly on the true interface.
      // After that, the local and global traction calculations remain the same as standard CZM.
      _disp_jump_provider_name = _use_kokkos ? "KokkosSCZMComputeDisplacementJumpSmallStrain"
                                             : (_use_AD ? "ADSCZMComputeDisplacementJumpSmallStrain"
                                                        : "SCZMComputeDisplacementJumpSmallStrain");
      _equilibrium_traction_calculator_name =
          _use_kokkos ? "KokkosCZMComputeGlobalTractionSmallStrain"
                      : (_use_AD ? "ADCZMComputeGlobalTractionSmallStrain"
                                 : "CZMComputeGlobalTractionSmallStrain");
      break;
    }
    case Strain::Finite:
    {
      _czm_kernel_name = _use_kokkos ? "KokkosSCZMInterfaceKernelTotalLagrangian"
                                     : (_use_AD ? "ADSCZMInterfaceKernelTotalLagrangian"
                                                : "SCZMInterfaceKernelTotalLagrangian");
      _disp_jump_provider_name = _use_kokkos
                                     ? "KokkosSCZMComputeDisplacementJumpTotalLagrangian"
                                     : (_use_AD ? "ADSCZMComputeDisplacementJumpTotalLagrangian"
                                                : "SCZMComputeDisplacementJumpTotalLagrangian");
      _equilibrium_traction_calculator_name =
          _use_kokkos ? "KokkosSCZMComputeGlobalTractionTotalLagrangian"
                      : (_use_AD ? "ADSCZMComputeGlobalTractionTotalLagrangian"
                                 : "SCZMComputeGlobalTractionTotalLagrangian");
      break;
    }
    default:
      mooseError(
          "ShiftedCohesiveZoneAction Error: Invalid kinematic parameter. Allowed values are: "
          "SmallStrain or TotalLagrangian");
  }
}

void
ShiftedCohesiveZoneAction::act()
{
  if (_current_task == "validate_coordinate_systems" && _use_kokkos)
    for (const auto subdomain : _problem->mesh().meshSubdomains())
      if (_problem->getCoordSystem(subdomain) != Moose::COORD_XYZ)
        paramError("use_kokkos",
                   "Kokkos SCZM directional correction currently supports only Cartesian "
                   "coordinate systems.");

  // Consistency check: ndisp must match mesh dimension
  if (_ndisp != _mesh->dimension())
    paramError("displacements", "Number of displacements must match problem dimension.");

  if (_current_task == "add_interface_kernel" && getParam<bool>("directional_correction"))
  {
    std::vector<MaterialPropertyName> stress_names;
    std::vector<MaterialPropertyName> tangent_names;

    if (isParamSetByUser("stress"))
      stress_names.push_back(getParam<MaterialPropertyName>("stress"));
    else if (_strain == Strain::Finite)
      stress_names.push_back("pk1_stress");
    else
    {
      // Mirror SCZMInterfaceKernelSmallStrain, which prefers stress/Jacobian_mult but falls back
      // to pk1_stress/pk1_jacobian for Lagrangian-compatible small-strain materials.
      stress_names.push_back("stress");
      stress_names.push_back("pk1_stress");
    }

    if (isParamSetByUser("tangent"))
      tangent_names.push_back(getParam<MaterialPropertyName>("tangent"));
    else if (_strain == Strain::Finite)
      tangent_names.push_back("pk1_jacobian");
    else
    {
      tangent_names.push_back("Jacobian_mult");
      tangent_names.push_back("pk1_jacobian");
    }

    // NEML2 properties already checked; candidate aliases may resolve to the same property
    std::vector<MaterialPropertyName> validated_neml2_properties;

    // Directional correction evaluates the adjacent continuum stress and tangent on the cohesive
    // interface. Native materials are reinitialized on interface faces automatically, but NEML2
    // evaluates only on its declared domains, so a NEML2 block providing the stress or tangent
    // must list the SCZM boundary in 'interface'.
    const auto validate_neml2_interface =
        [this, &validated_neml2_properties](const MaterialPropertyName & property_name)
    {
      // Trace PK1 names created by ComputeLagrangianCauchyCustomStress back to the NEML2 name
      const auto neml2_property_name = resolveTransferredProperty(_awh, property_name);

      // In small strain, "stress" and "pk1_stress" can resolve to the same Cauchy stress; check
      // it once to avoid duplicate traversals and errors
      if (std::find(validated_neml2_properties.begin(),
                    validated_neml2_properties.end(),
                    neml2_property_name) != validated_neml2_properties.end())
        return;

      validated_neml2_properties.push_back(neml2_property_name);

      for (const auto * const neml2_action : _awh.getActions<NEML2Action>())
      {
        // Check every [NEML2] block that produces the property, not only the first one
        if (!neml2_action->producesProperty(neml2_property_name))
          continue;

        // The Kokkos interface kernel reads device material properties only
        if (_use_kokkos && neml2_action->getParam<MooseEnum>("output_backend") != "kokkos")
          neml2_action->paramError(
              "output_backend",
              "The material property '",
              neml2_property_name,
              "' produced by this NEML2 block is required on the cohesive interface by a Kokkos "
              "[Physics/SolidMechanics/ShiftedCohesiveZone]. Set 'output_backend = kokkos' or set "
              "'directional_correction = false' in the ShiftedCohesiveZone block.");

        const bool interface_set = neml2_action->isParamSetByUser("interface");
        const auto & interfaces = neml2_action->getParam<std::vector<BoundaryName>>("interface");

        if (!interface_set || interfaces.empty())
          neml2_action->paramError(
              "interface",
              "The material property '",
              neml2_property_name,
              "' produced by this NEML2 block is required on the cohesive interface by "
              "[Physics/SolidMechanics/ShiftedCohesiveZone] with directional_correction = true. "
              "This NEML2 block needs 'interface' set to the SCZM boundary (for example, "
              "interface = ${boundary}) or set 'directional_correction = false' in the "
              "ShiftedCohesiveZone block.");

        if (!hasOverlap(_boundary, interfaces))
          neml2_action->paramError(
              "interface",
              "The material property '",
              neml2_property_name,
              "' produced by this NEML2 block is required on the cohesive interface by "
              "[Physics/SolidMechanics/ShiftedCohesiveZone] with directional_correction = true, "
              "but its 'interface' setting does not overlap the SCZM boundary. This NEML2 block "
              "needs 'interface' to include the SCZM boundary or set 'directional_correction = "
              "false' in the ShiftedCohesiveZone block.");
      }
    };

    // Check every stress and tangent name the interface kernel may consume
    for (const auto & stress_name : stress_names)
      validate_neml2_interface(stress_name);

    for (const auto & tangent_name : tangent_names)
      validate_neml2_interface(tangent_name);
  }

  // Call the base utility for parameter validation across multiple blocks
  chekMultipleActionParameters();

  // Task Routing
  if (_current_task == "add_function" && getParam<bool>("generate_sbm_distance"))
    addSBMDistanceFunctions();
  else if (_current_task == "add_user_object" &&
           (getParam<bool>("generate_sbm_distance") || usesInterfaceManager()))
    addSBMDistanceUserObjects();
  else if (_current_task == "add_interface_kernel")
    addRequiredCZMInterfaceKernels();
  else if (_current_task == "add_master_action_material")
  {
    if (needsPK2ToPK1Conversion())
      addPK2ToPK1InterfaceConverters();
    addRequiredCZMInterfaceMaterials();
  }

  // optional, add required outputs
  actOutputGeneration();
}

std::vector<MeshGeneratorName>
ShiftedCohesiveZoneAction::surfaceMeshNames() const
{
  if (isParamValid("surface_meshes"))
    return getParam<std::vector<MeshGeneratorName>>("surface_meshes");

  std::vector<MeshGeneratorName> surface_meshes;
  surface_meshes.reserve(_boundary.size());
  for (const auto & boundary : _boundary)
    surface_meshes.push_back(boundary);
  return surface_meshes;
}

UserObjectName
ShiftedCohesiveZoneAction::surfaceMeshBuilderName(const BoundaryName & boundary) const
{
  return name() + "_" + boundary + "_builder";
}

FunctionName
ShiftedCohesiveZoneAction::surfaceDistanceFunctionName(const BoundaryName & boundary) const
{
  return name() + "_dist_" + boundary;
}

UserObjectName
ShiftedCohesiveZoneAction::sbmDistanceUserObjectName() const
{
  return name() + "_sbm_distance";
}

UserObjectName
ShiftedCohesiveZoneAction::interfaceManagerName() const
{
  return name() + "_interface_manager";
}

bool
ShiftedCohesiveZoneAction::usesInterfaceManager() const
{
  return isParamSetByUser("complete_interface_mesh") ||
         isParamSetByUser("complete_interface_mesh_file") ||
         isParamSetByUser("complete_interface_meshes");
}

void
ShiftedCohesiveZoneAction::addSBMDistanceFunctions()
{
  for (const auto & boundary : _boundary)
  {
    auto params = _factory.getValidParams("UnsignedDistanceToSurfaceMesh");
    params.set<UserObjectName>("builder") = surfaceMeshBuilderName(boundary);
    _problem->addFunction(
        "UnsignedDistanceToSurfaceMesh", surfaceDistanceFunctionName(boundary), params);
  }
}

void
ShiftedCohesiveZoneAction::addSBMDistanceUserObjects()
{
  auto distance_params = _factory.getValidParams("BoundaryShortestDistanceToSurface");
  distance_params.set<std::vector<BoundaryName>>("boundary") = _boundary;
  // Kokkos user objects run before host user objects within an execution order group, and a
  // Kokkos user object can evaluate the shifted Kokkos materials (e.g. a NEML2 gatherer of the
  // displacement jump), so the distance must come from an earlier group
  distance_params.set<int>("execution_order_group") = _use_kokkos ? -2 : 0;
  distance_params.set<ExecFlagEnum>("execute_on") = EXEC_INITIAL;

  if (usesInterfaceManager())
  {
    auto manager_params = _factory.getValidParams("SBMInterfaceManager");
    if (isParamSetByUser("complete_interface_mesh"))
      manager_params.set<MeshGeneratorName>("complete_interface_mesh") =
          getParam<MeshGeneratorName>("complete_interface_mesh");
    else if (isParamSetByUser("complete_interface_mesh_file"))
      manager_params.set<FileName>("complete_interface_mesh_file") =
          getParam<FileName>("complete_interface_mesh_file");
    else
    {
      // Per-grain list form: the manager stitches the shells itself. These are
      // the base BoundaryMeshBuilder parameter names (not renamed by the manager).
      manager_params.set<std::vector<MeshGeneratorName>>("boundary_meshes") =
          getParam<std::vector<MeshGeneratorName>>("complete_interface_meshes");
      manager_params.set<std::vector<subdomain_id_type>>("subdomain_ids") =
          getParam<std::vector<subdomain_id_type>>("interface_subdomain_ids");
    }
    manager_params.set<Real>("tolerance") = getParam<Real>("interface_matching_tolerance");
    // The manager detects the interfaces that the distance user object reads in its own setup
    manager_params.set<int>("execution_order_group") =
        distance_params.get<int>("execution_order_group") - 1;
    _problem->addUserObject("SBMInterfaceManager", interfaceManagerName(), manager_params);
    distance_params.set<UserObjectName>("manager") = interfaceManagerName();
    if (isParamSetByUser("interface_subdomain_pairs"))
      distance_params.set<std::vector<std::vector<SubdomainID>>>("interface_subdomain_pairs") =
          getParam<std::vector<std::vector<SubdomainID>>>("interface_subdomain_pairs");
  }
  else
  {
    const auto surface_meshes = surfaceMeshNames();
    std::vector<FunctionName> distance_functions;
    distance_functions.reserve(_boundary.size());
    for (const auto i : index_range(_boundary))
    {
      auto params = _factory.getValidParams("SBMSurfaceMeshBuilder");
      params.set<MeshGeneratorName>("interface_mesh") = surface_meshes[i];
      params.set<bool>("check_watertightness") = getParam<bool>("check_surface_watertightness");
      _problem->addUserObject(
          "SBMSurfaceMeshBuilder", surfaceMeshBuilderName(_boundary[i]), params);
      distance_functions.push_back(surfaceDistanceFunctionName(_boundary[i]));
    }
    distance_params.set<std::vector<FunctionName>>("surfaces") = distance_functions;
  }

  _problem->addUserObject(
      "BoundaryShortestDistanceToSurface", sbmDistanceUserObjectName(), distance_params);
}

bool
ShiftedCohesiveZoneAction::needsPK2ToPK1Conversion() const
{
  return getParam<bool>("directional_correction") && !getParam<bool>("no_shifted") &&
         getParam<MooseEnum>("tangent_definition") == "pk2_wrt_deformation_gradient";
}

void
ShiftedCohesiveZoneAction::addPK2ToPK1InterfaceConverters()
{
  if (!_use_kokkos)
    paramError("use_kokkos",
               "Automatic PK2-to-PK1 conversion for directional correction is not yet supported "
               "on the CPU path.");

#ifdef MOOSE_KOKKOS_ENABLED
  // For large-deformation models, NEML2 provides PK2 stress and dPK2/dF on both sides
  // of the interface. Convert them to PK1 stress and dPK1/dF for the SCZM directional correction.
  const auto pk1_stress = _name + "_pk1_stress";
  const auto pk1_jacobian = _name + "_pk1_jacobian";
  const auto add_converter =
      [this, &pk1_stress, &pk1_jacobian](const std::string & type, const std::string & suffix)
  {
    auto params = _factory.getValidParams(type);
    params.set<std::vector<BoundaryName>>("boundary") = _boundary;
    params.set<std::vector<VariableName>>("displacements") = _displacements;
    params.set<MaterialPropertyName>("custom_pk2_stress") =
        getParam<MaterialPropertyName>("stress");
    params.set<MaterialPropertyName>("custom_pk2_jacobian") =
        getParam<MaterialPropertyName>("tangent");
    params.set<MaterialPropertyName>("pk1_stress") = pk1_stress;
    params.set<MaterialPropertyName>("dpk1_d_grad_u") = pk1_jacobian;
    _problem->addKokkosMaterial(type, _name + "_pk2_to_pk1_" + suffix, params);
  };
  add_converter("KokkosInterfaceComputeLagrangianStressCustomPK2", "primary");
  add_converter("KokkosNeighborComputeLagrangianStressCustomPK2", "neighbor");
#else
  mooseError("Kokkos SCZM was requested, but this MOOSE build does not enable Kokkos.");
#endif
}

void
ShiftedCohesiveZoneAction::addRequiredCZMInterfaceKernels()
{
  if (!_use_kokkos)
  {
    CohesiveZoneAction::addRequiredCZMInterfaceKernels();
    return;
  }

#ifdef MOOSE_KOKKOS_ENABLED
  for (const auto i : make_range(_ndisp))
  {
    const auto unique_kernel_name = _czm_kernel_name + "_" + _name + "_" + Moose::stringify(i);
    auto params = _factory.getValidParams(_czm_kernel_name);

    params.set<unsigned int>("component") = i;
    params.set<NonlinearVariableName>("variable") = _displacements[i];
    params.set<std::vector<VariableName>>("neighbor_var") = {_displacements[i]};
    params.set<std::vector<VariableName>>("displacements") = _displacements;
    params.set<std::vector<BoundaryName>>("boundary") = _boundary;
    params.set<std::string>("base_name") = _base_name;
    customizeCZMInterfaceKernelParameters(params);
    if (needsPK2ToPK1Conversion())
    {
      params.set<MaterialPropertyName>("stress") = _name + "_pk1_stress";
      params.set<MaterialPropertyName>("tangent") = _name + "_pk1_jacobian";
      params.set<MooseEnum>("tangent_definition") = "pk1_wrt_deformation_gradient";
    }

    std::string save_in_side;
    std::vector<AuxVariableName> save_in_var_names;
    if (_save_in_primary.size() == _ndisp || _save_in_secondary.size() == _ndisp)
    {
      prepareSaveInInputs(save_in_var_names, save_in_side, _save_in_primary, _save_in_secondary, i);
      params.set<std::vector<AuxVariableName>>("save_in") = save_in_var_names;
      params.set<MultiMooseEnum>("save_in_var_side") = save_in_side;
    }
    if (_diag_save_in_primary.size() == _ndisp || _diag_save_in_secondary.size() == _ndisp)
    {
      prepareSaveInInputs(
          save_in_var_names, save_in_side, _diag_save_in_primary, _diag_save_in_secondary, i);
      params.set<std::vector<AuxVariableName>>("diag_save_in") = save_in_var_names;
      params.set<MultiMooseEnum>("diag_save_in_var_side") = save_in_side;
    }

    _problem->addKokkosInterfaceKernel(_czm_kernel_name, unique_kernel_name, params);
  }
#else
  mooseError("Kokkos SCZM was requested, but this MOOSE build does not enable Kokkos.");
#endif
}

void
ShiftedCohesiveZoneAction::addRequiredCZMInterfaceMaterials()
{
  if (!_use_kokkos)
  {
    CohesiveZoneAction::addRequiredCZMInterfaceMaterials();
    return;
  }

#ifdef MOOSE_KOKKOS_ENABLED
  auto params = _factory.getValidParams(_disp_jump_provider_name);
  params.set<std::vector<BoundaryName>>("boundary") = _boundary;
  params.set<std::vector<VariableName>>("displacements") = _displacements;
  params.set<std::string>("base_name") = _base_name;
  customizeCZMDisplacementJumpMaterialParameters(params);
  _problem->addKokkosMaterial(
      _disp_jump_provider_name, _disp_jump_provider_name + "_" + _name, params);

  params = _factory.getValidParams(_equilibrium_traction_calculator_name);
  params.set<std::vector<BoundaryName>>("boundary") = _boundary;
  params.set<std::string>("base_name") = _base_name;
  customizeCZMGlobalTractionMaterialParameters(params);
  _problem->addKokkosMaterial(_equilibrium_traction_calculator_name,
                              _equilibrium_traction_calculator_name + "_" + _name,
                              params);
#else
  mooseError("Kokkos SCZM was requested, but this MOOSE build does not enable Kokkos.");
#endif
}

void
ShiftedCohesiveZoneAction::customizeCZMInterfaceKernelParameters(InputParameters & params) const
{
  if (getParam<bool>("generate_sbm_distance") || usesInterfaceManager())
    params.set<UserObjectName>("sbm_distance_uo") = sbmDistanceUserObjectName();
  else if (isParamSetByUser("sbm_distance_uo"))
    params.set<UserObjectName>("sbm_distance_uo") = getParam<UserObjectName>("sbm_distance_uo");
  if (isParamSetByUser("no_shifted") && params.isParamValid("no_shifted"))
    params.set<bool>("no_shifted") = getParam<bool>("no_shifted");
  if (params.isParamValid("directional_correction"))
    params.set<bool>("directional_correction") = getParam<bool>("directional_correction");
  if (params.isParamValid("stress"))
    params.set<MaterialPropertyName>("stress") =
        _strain == Strain::Finite && !isParamSetByUser("stress")
            ? MaterialPropertyName("pk1_stress")
            : getParam<MaterialPropertyName>("stress");
  if (params.isParamValid("tangent"))
    params.set<MaterialPropertyName>("tangent") =
        _strain == Strain::Finite && !isParamSetByUser("tangent")
            ? MaterialPropertyName("pk1_jacobian")
            : getParam<MaterialPropertyName>("tangent");
  if (params.isParamValid("stabilized_tangent"))
    params.set<MaterialPropertyName>("stabilized_tangent") =
        getParam<MaterialPropertyName>("stabilized_tangent");
  if (params.isParamValid("tangent_definition"))
  {
    if (isParamSetByUser("tangent_definition"))
      params.set<MooseEnum>("tangent_definition") = getParam<MooseEnum>("tangent_definition");
    else if (_strain == Strain::Finite)
      params.set<MooseEnum>("tangent_definition") = "pk1_wrt_deformation_gradient";
  }

  if (params.isParamValid("volumetric_locking_correction"))
  {
    const bool inherited_correction = getInheritedVolumetricLockingCorrection(_awh);
    if (isParamSetByUser("volumetric_locking_correction"))
    {
      params.set<bool>("volumetric_locking_correction") =
          getParam<bool>("volumetric_locking_correction");
      if (hasInheritedVolumetricLockingCorrectionSetting(_awh) &&
          getParam<bool>("volumetric_locking_correction") != inherited_correction)
        mooseWarning(
            "ShiftedCohesiveZoneAction: 'volumetric_locking_correction' value in "
            "ShiftedCohesiveZone block is different from that in the physics block(s). Make sure "
            "this is intended.");
    }
    else
      params.set<bool>("volumetric_locking_correction") = inherited_correction;
  }
}

void
ShiftedCohesiveZoneAction::customizeCZMDisplacementJumpMaterialParameters(
    InputParameters & params) const
{
  if (params.have_parameter<UserObjectName>("sbm_distance_uo"))
  {
    if (getParam<bool>("generate_sbm_distance") || usesInterfaceManager())
      params.set<UserObjectName>("sbm_distance_uo") = sbmDistanceUserObjectName();
    else if (isParamSetByUser("sbm_distance_uo"))
      params.set<UserObjectName>("sbm_distance_uo") = getParam<UserObjectName>("sbm_distance_uo");
  }
  if (isParamSetByUser("no_shifted") && params.isParamValid("no_shifted"))
    params.set<bool>("no_shifted") = getParam<bool>("no_shifted");
}

void
ShiftedCohesiveZoneAction::customizeCZMGlobalTractionMaterialParameters(
    InputParameters & params) const
{
  customizeCZMDisplacementJumpMaterialParameters(params);
}
