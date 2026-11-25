//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSCZMInterfaceKernelTotalLagrangian.h"

#include "KokkosInterfaceDatum.h"
#include "FEProblemBase.h"

#include "libmesh/fe_map.h"

#include <cmath>
#include <vector>

registerKokkosInterfaceKernel("ShiftedBoundaryMethodApp", KokkosSCZMInterfaceKernelTotalLagrangian);

InputParameters
KokkosSCZMInterfaceKernelTotalLagrangian::validParams()
{
  auto params = ShiftedInterfaceKernel::validParams();
  params.addRequiredParam<unsigned int>("component",
                                        "The displacement component assembled by this kernel.");
  params.addRequiredCoupledVar("displacements", "The displacement variables.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addParam<MaterialPropertyName>(
      "traction_global", "PK1traction", "Global PK1 traction material property.");
  params.addParam<MaterialPropertyName>(
      "dtraction_djump_global",
      "dtraction_djump_global",
      "Derivative of global PK1 traction with respect to global jump.");
  params.addParam<MaterialPropertyName>(
      "dPK1traction_dF",
      "dPK1traction_dF",
      "Derivative of global PK1 traction with respect to deformation gradient.");
  params.addParam<bool>(
      "no_shifted",
      false,
      "Disable shifted terms. When enabled the base traction omits the area correction and the "
      "directional correction, and the traction-jump Jacobian uses the natural test functions.");
  params.addParam<bool>("directional_correction", true, "Add the directional correction terms.");
  params.addParam<MooseEnum>(
      "stress_recovery",
      MooseEnum("none element_linear", "none"),
      "Optional element-local affine recovery of each side's PK1 stress from its volume quadrature "
      "points to the true-interface quadrature point.");
  params.addParam<bool>(
      "volumetric_locking_correction",
      false,
      "Use the total-mode F-bar PK1 stress P(U, B) in the directional correction. The 'tangent' "
      "property is then d(P)/d(U) at fixed B, and the Jacobian adds the local and element-volume "
      "F-bar terms through B = (det(M) / det(U))^(1/3) U.");
  params.addParam<MaterialPropertyName>(
      "stress", "pk1_stress", "Interface PK1 stress material property.");
  params.addParam<MaterialPropertyName>(
      "tangent", "pk1_jacobian", "Interface constitutive tangent material property (d(PK1)/d(F)).");
  params.addParam<MaterialPropertyName>(
      "volume_stress", "pk1_stress", "Volume PK1 stress used by element-linear recovery.");
  params.addParam<MaterialPropertyName>(
      "volume_tangent",
      "dpk1_d_grad_u",
      "Complete local volume derivative d(PK1)/d(grad(u)) used by element-linear recovery.");
  params.addParam<MaterialPropertyName>("volume_F_avg_invT",
                                        "F_avg_invT",
                                        "Inverse transpose of the element-average deformation "
                                        "gradient used by recovered F-bar stress.");
  params.addParam<MaterialPropertyName>(
      "volume_fbar_A", "fbar_A", "Volume F-bar A(qp) tensor used by recovered F-bar stress.");
  params.addParam<MaterialPropertyName>(
      "volume_fbar_gamma_over_3",
      "fbar_gamma_over_3",
      "Volume F-bar gamma/3 scalar used by recovered F-bar stress.");
  params.addParam<MaterialPropertyName>(
      "stabilized_tangent",
      "stabilized_pk1_jacobian",
      "Derivative of the PK1 stress with respect to the F-bar deformation gradient B at fixed "
      "physical deformation gradient U, used with volumetric_locking_correction.");
  params.addParam<MaterialPropertyName>(
      "physical_deformation_gradient",
      "physical_deformation_gradient",
      "Physical deformation gradient U, used with volumetric_locking_correction.");
  params.addParam<MaterialPropertyName>(
      "average_deformation_gradient",
      "average_deformation_gradient",
      "Average M of the physical deformation gradient over the owning element volume, used with "
      "volumetric_locking_correction.");
  params.addParam<UserObjectName>(
      "stabilization_data",
      "Shared weighted-traction bulk stress, automatic beta, and recovery data producer.");
  params.addParam<MooseEnum>("tangent_definition",
                             MooseEnum("auto stress_wrt_strain pk1_wrt_deformation_gradient",
                                       "pk1_wrt_deformation_gradient"),
                             "Mathematical definition of the tangent material property.");
  SCZMWeightedTractionStabilization::addParams(params);
  params.addParam<MooseEnum>("stabilization_cohesive_law",
                             MooseEnum("affine coupled_cubic", "affine"),
                             "Native memoryless cohesive evaluator used by generalized Robin "
                             "stabilization.");
  params.addParam<Real>("stabilization_linear_stiffness", "Coupled-cubic linear coefficient.");
  params.addParam<Real>("stabilization_cubic_stiffness", "Coupled-cubic cubic coefficient.");
  params.addClassDescription("Kokkos shifted cohesive-zone interface kernel for total-Lagrangian "
                             "finite-strain displacements.");
  return params;
}

KokkosSCZMInterfaceKernelTotalLagrangian::KokkosSCZMInterfaceKernelTotalLagrangian(
    const InputParameters & parameters)
  : ShiftedInterfaceKernel(parameters),
    _component(getParam<unsigned int>("component")),
    _ndisp(coupledComponents("displacements")),
    _shifted(!getParam<bool>("no_shifted")),
    _directional_correction(getParam<bool>("directional_correction")),
    _volumetric_locking_correction(getParam<bool>("volumetric_locking_correction")),
    _element_linear_recovery(getParam<MooseEnum>("stress_recovery") == "element_linear"),
    _coupled_cubic_stabilization(getParam<MooseEnum>("stabilization_cohesive_law") ==
                                 "coupled_cubic"),
    _stabilization_settings(SCZMWeightedTractionStabilization::settings(*this)),
    _stabilization_data(isParamValid("stabilization_data")
                            ? &const_cast<KokkosSCZMStabilizationData &>(
                                  getUserObject<KokkosSCZMStabilizationData>("stabilization_data"))
                            : nullptr),
    _displacement_jump_global(getKokkosMaterialPropertyByName<Real, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "displacement_jump_global")),
    _czm_total_rotation(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "czm_total_rotation")),
    _czm_reference_rotation(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "czm_reference_rotation")),
    _czm_rotation(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "czm_rotation")),
    _F_czm(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "F_czm")),
    _interface_traction(getKokkosMaterialPropertyByName<Real, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "interface_traction")),
    _dinterface_traction_djump(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "dinterface_traction_djump")),
    _traction_global(getKokkosMaterialPropertyByName<Real, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("traction_global"))),
    _dtraction_djump_global(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("dtraction_djump_global"))),
    _dPK1traction_dF(getKokkosMaterialPropertyByName<Real, 3>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("dPK1traction_dF")))
{
  if (_ndisp != _mesh.dimension())
    paramError("displacements", "Number of displacements must match problem dimension.");
  if (_ndisp < 1 || _ndisp > 3)
    paramError("displacements", "One, two, or three displacement variables are required.");
  if (_component >= _ndisp)
    paramError("component", "Component must be smaller than the number of displacements.");
  if (_stabilization_settings.enabled && !_stabilization_data)
    paramError("stabilization_data",
               "Weighted-traction stabilization requires a shared stabilization data producer.");
  if (_coupled_cubic_stabilization)
  {
    if (!_stabilization_settings.enabled)
      paramError("stabilization_cohesive_law",
                 "The coupled_cubic evaluator requires weighted traction stabilization.");
  }

  _disp_vars.create(_ndisp);
  for (unsigned int i = 0; i < _ndisp; ++i)
    _disp_vars[i] = coupled("displacements", i);
  _disp_vars.copyToDevice();

  if (_shifted && !isParamValid("sbm_distance_uo"))
    paramError("sbm_distance_uo",
               "The 'sbm_distance_uo' parameter is required unless 'no_shifted = true'.");
  if (_element_linear_recovery && !_stabilization_settings.enabled)
    paramError("stress_recovery",
               "'element_linear' requires weighted traction stabilization because recovery applies "
               "to its bulk PK1 stress contribution.");
  if (_element_linear_recovery && !_shifted)
    paramError("stress_recovery", "'element_linear' requires the true-interface mapping.");

  if (_element_linear_recovery ||
      (_stabilization_settings.enabled && !_stabilization_settings.beta_override))
  {
    auto & volume_material_data =
        _fe_problem.getKokkosMaterialData(Moose::BLOCK_MATERIAL_DATA, this);
    const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                            ? getParam<std::string>("base_name") + "_"
                            : "";
    const auto volume_tangent = prefix + getParam<MaterialPropertyName>("volume_tangent");
    _volume_tangent =
        getKokkosMaterialPropertyByName<Real, 4>(volume_tangent, volume_material_data);
    auto & volume_storage = _fe_problem.getKokkosMaterialPropertyStorage();
    volume_storage.requestGhostedProperty(volume_tangent);

    if (_element_linear_recovery)
    {
      const auto volume_stress = prefix + getParam<MaterialPropertyName>("volume_stress");
      _volume_stress =
          getKokkosMaterialPropertyByName<Real, 2>(volume_stress, volume_material_data);
      volume_storage.requestGhostedProperty(volume_stress);
    }
    if (_element_linear_recovery && _volumetric_locking_correction)
    {
      const auto F_avg_invT = prefix + getParam<MaterialPropertyName>("volume_F_avg_invT");
      const auto fbar_A = prefix + getParam<MaterialPropertyName>("volume_fbar_A");
      const auto fbar_gamma_over_3 =
          prefix + getParam<MaterialPropertyName>("volume_fbar_gamma_over_3");
      _volume_F_avg_invT =
          getKokkosMaterialPropertyByName<Real, 2>(F_avg_invT, volume_material_data);
      _volume_fbar_A = getKokkosMaterialPropertyByName<Real, 2>(fbar_A, volume_material_data);
      _volume_fbar_gamma_over_3 =
          getKokkosMaterialPropertyByName<Real, 0>(fbar_gamma_over_3, volume_material_data);
      volume_storage.requestGhostedProperty(F_avg_invT);
      volume_storage.requestGhostedProperty(fbar_A);
      volume_storage.requestGhostedProperty(fbar_gamma_over_3);
    }
  }

  // Recovery replaces both true-interface stabilization stress and surrogate-face directional
  // stress
  if (!_element_linear_recovery &&
      ((_shifted && _directional_correction) || _stabilization_settings.enabled))
  {
    const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                            ? getParam<std::string>("base_name") + "_"
                            : "";
    _stress =
        getKokkosMaterialPropertyByName<Real, 2>(prefix + getParam<MaterialPropertyName>("stress"));
    _stress_neighbor = getKokkosNeighborMaterialPropertyByName<Real, 2>(
        prefix + getParam<MaterialPropertyName>("stress"));
    _tangent = getKokkosMaterialPropertyByName<Real, 4>(prefix +
                                                        getParam<MaterialPropertyName>("tangent"));
    _tangent_neighbor = getKokkosNeighborMaterialPropertyByName<Real, 4>(
        prefix + getParam<MaterialPropertyName>("tangent"));

    if (_volumetric_locking_correction)
    {
      if (getParam<MooseEnum>("tangent_definition") != "pk1_wrt_deformation_gradient")
        paramError("tangent_definition",
                   "The total-Lagrangian F-bar correction requires "
                   "'pk1_wrt_deformation_gradient'.");

      const auto stabilized_tangent = prefix + getParam<MaterialPropertyName>("stabilized_tangent");
      const auto physical =
          prefix + getParam<MaterialPropertyName>("physical_deformation_gradient");
      const auto average = prefix + getParam<MaterialPropertyName>("average_deformation_gradient");
      _stabilized_tangent = getKokkosMaterialPropertyByName<Real, 4>(stabilized_tangent);
      _stabilized_tangent_neighbor =
          getKokkosNeighborMaterialPropertyByName<Real, 4>(stabilized_tangent);
      _physical_deformation_gradient = getKokkosMaterialPropertyByName<Real, 2>(physical);
      _physical_deformation_gradient_neighbor =
          getKokkosNeighborMaterialPropertyByName<Real, 2>(physical);
      _average_deformation_gradient = getKokkosMaterialPropertyByName<Real, 2>(average);
      _average_deformation_gradient_neighbor =
          getKokkosNeighborMaterialPropertyByName<Real, 2>(average);
    }
  }
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::initialSetup()
{
  ShiftedInterfaceKernel::initialSetup();
  if (!_stabilization_settings.enabled)
    return;

  const auto & producer_settings = _stabilization_data->stabilizationSettings();
  const bool beta_mismatch =
      producer_settings.beta_override != _stabilization_settings.beta_override ||
      (producer_settings.beta_override && producer_settings.beta != _stabilization_settings.beta);
  const bool cohesive_law_mismatch =
      _stabilization_data->coupledCubicStabilization() != _coupled_cubic_stabilization ||
      (_coupled_cubic_stabilization &&
       (_stabilization_data->stabilizationLinearStiffness() !=
            getParam<Real>("stabilization_linear_stiffness") ||
        _stabilization_data->stabilizationCubicStiffness() !=
            getParam<Real>("stabilization_cubic_stiffness")));
  if (beta_mismatch || producer_settings.gamma != _stabilization_settings.gamma ||
      cohesive_law_mismatch ||
      _stabilization_data->elementLinearRecovery() != _element_linear_recovery ||
      _stabilization_data->shifted() != _shifted)
    mooseError("SCZM kernel and stabilization producer configurations differ.");

  dof_id_type n_qps = 0;
  for (const auto & face : kokkosInterfaceFaces())
    n_qps += face.n_qps;
  if (_stabilization_data->numFaces() != kokkosInterfaceFaces().size() ||
      _stabilization_data->numQps() != n_qps)
    mooseError("SCZM kernel and stabilization producer interface face/QP layouts differ.");

  _face_qp_offsets = _stabilization_data->faceQpOffsets();
  _stabilization_beta = _stabilization_data->stabilizationBetas();
  _stabilized_traction = _stabilization_data->stabilizedTractions();
  _robin_W = _stabilization_data->robinTangents();
  _robin_weighted_dF = _stabilization_data->robinWeightedDeformationGradientDerivatives();
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::residualSetup()
{
  ShiftedInterfaceKernel::residualSetup();
  if (_stabilization_data)
    _stabilization_data->invalidateAssemblyCache();
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::jacobianSetup()
{
  ShiftedInterfaceKernel::jacobianSetup();
  if (_stabilization_data)
    _stabilization_data->invalidateAssemblyCache();
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::computeResidual()
{
  ensureSurrogateInterfaceMappingReady();
  prepareStabilization(_fe_problem.currentlyComputingResidualAndJacobian());
  _residual_dispatcher.reset();
  InterfaceKernel::computeResidual();
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::computeJacobian()
{
  ensureSurrogateInterfaceMappingReady();
  prepareStabilization(true);
  _jacobian_dispatcher.reset();
  _offdiag_jacobian_dispatcher.reset();
  InterfaceKernel::computeJacobian();
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::prepareStabilization(const bool need_tangents)
{
  if (!_stabilization_data)
    return;

  _stabilization_data->ensurePrepared(need_tangents);
  // The producer builds the recovery arrays on its first preparation
  if (_element_linear_recovery && !_recovery_initialized)
  {
    _recovery_offsets = _stabilization_data->recoveryOffsets();
    _recovery_weights = _stabilization_data->hostRecoveryWeights();
    _recovery_initialized = true;
  }
}
