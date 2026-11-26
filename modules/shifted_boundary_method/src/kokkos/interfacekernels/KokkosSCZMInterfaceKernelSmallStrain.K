//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSCZMInterfaceKernelSmallStrain.h"

registerKokkosInterfaceKernel("ShiftedBoundaryMethodApp", KokkosSCZMInterfaceKernelSmallStrain);

InputParameters
KokkosSCZMInterfaceKernelSmallStrain::validParams()
{
  auto params = ShiftedInterfaceKernel::validParams();
  params.addRequiredParam<unsigned int>("component",
                                        "The displacement component assembled by this kernel.");
  params.addRequiredCoupledVar("displacements", "The displacement variables.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addParam<MaterialPropertyName>(
      "traction_global", "traction_global", "Global traction material property.");
  params.addParam<MaterialPropertyName>(
      "dtraction_djump_global",
      "dtraction_djump_global",
      "Derivative of global traction with respect to global jump.");
  params.addParam<bool>(
      "no_shifted",
      false,
      "Disable shifted terms. When enabled the base traction omits the area correction and the "
      "directional correction, and the traction-jump Jacobian uses the natural test functions.");
  params.addParam<bool>("directional_correction", true, "Add the directional correction terms.");
  params.addParam<bool>(
      "volumetric_locking_correction",
      false,
      "Apply the bulk-consistent volumetric locking correction to the directional "
      "Jacobian blocks.");
  params.addParam<MaterialPropertyName>("stress", "stress", "Stress material property.");
  params.addParam<MaterialPropertyName>(
      "tangent", "Jacobian_mult", "Constitutive tangent material property.");
  params.addParam<MooseEnum>(
      "tangent_definition",
      MooseEnum("auto stress_wrt_strain pk1_wrt_deformation_gradient", "auto"),
      "Mathematical definition of the tangent material property. 'auto' recognizes the standard "
      "Jacobian_mult property as d(stress)/d(strain) and pk1_jacobian as d(PK1)/d(F). Select "
      "'stress_wrt_strain' or 'pk1_wrt_deformation_gradient' explicitly for a custom tangent "
      "property; use the latter only when the property is known to be d(PK1)/d(F).");
  params.addParam<UserObjectName>(
      "stabilization_data",
      "Shared weighted-traction bulk stress, automatic beta, and generalized Robin data producer.");
  SCZMWeightedTractionStabilization::addParams(params);
  params.addClassDescription(
      "Kokkos shifted cohesive-zone interface kernel for small-strain displacements.");
  return params;
}

KokkosSCZMInterfaceKernelSmallStrain::KokkosSCZMInterfaceKernelSmallStrain(
    const InputParameters & parameters)
  : ShiftedInterfaceKernel(parameters),
    _component(getParam<unsigned int>("component")),
    _ndisp(coupledComponents("displacements")),
    _shifted(!getParam<bool>("no_shifted")),
    _directional_correction(getParam<bool>("directional_correction")),
    _volumetric_locking_correction(getParam<bool>("volumetric_locking_correction")),
    _stabilization_settings(SCZMWeightedTractionStabilization::settings(*this)),
    _stabilization_data(isParamValid("stabilization_data")
                            ? &const_cast<KokkosSCZMStabilizationData &>(
                                  getUserObject<KokkosSCZMStabilizationData>("stabilization_data"))
                            : nullptr),
    _tangent_is_dpk1_df(false),
    _traction_global(getKokkosMaterialPropertyByName<Real, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("traction_global"))),
    _dtraction_djump_global(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("dtraction_djump_global")))
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

  _disp_vars.create(_ndisp);
  for (unsigned int i = 0; i < _ndisp; ++i)
    _disp_vars[i] = coupled("displacements", i);
  _disp_vars.copyToDevice();

  const auto & tangent_name = getParam<MaterialPropertyName>("tangent");
  const auto & tangent_definition = getParam<MooseEnum>("tangent_definition");
  if (tangent_definition == "auto")
  {
    if (tangent_name == "Jacobian_mult")
      _tangent_is_dpk1_df = false;
    else if (tangent_name == "pk1_jacobian")
      _tangent_is_dpk1_df = true;
    else
      paramError("tangent_definition",
                 "Cannot infer the mathematical definition of custom tangent property '",
                 tangent_name,
                 "'. Set 'tangent_definition' to 'stress_wrt_strain' or "
                 "'pk1_wrt_deformation_gradient'.");
  }
  else
    _tangent_is_dpk1_df = tangent_definition == "pk1_wrt_deformation_gradient";

  if (_shifted && !isParamValid("sbm_distance_uo"))
    paramError("sbm_distance_uo",
               "The 'sbm_distance_uo' parameter is required unless 'no_shifted = true'.");

  if ((_shifted && _directional_correction) || _stabilization_settings.enabled)
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

    // The passive stabilization producer reads these interface properties, but
    // KokkosNonlinearSystemBase gathers needed properties only from kernels, so the kernel must
    // request them for the producer.
    if (_stabilization_settings.enabled)
    {
      getKokkosMaterialPropertyByName<Real, 1>(prefix + "displacement_jump_global");
      getKokkosMaterialPropertyByName<Real, 2>(prefix + "czm_total_rotation");
      getKokkosMaterialPropertyByName<Real, 1>(prefix + "interface_traction");
      getKokkosMaterialPropertyByName<Real, 2>(prefix + "dinterface_traction_djump");
    }
  }
}

void
KokkosSCZMInterfaceKernelSmallStrain::initialSetup()
{
  ShiftedInterfaceKernel::initialSetup();
  if (!_stabilization_settings.enabled)
    return;

  const auto & producer_settings = _stabilization_data->stabilizationSettings();
  if (producer_settings.gamma != _stabilization_settings.gamma ||
      _stabilization_data->shifted() != _shifted || _stabilization_data->finiteStrain())
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
}

void
KokkosSCZMInterfaceKernelSmallStrain::residualSetup()
{
  ShiftedInterfaceKernel::residualSetup();
  if (_stabilization_data)
    _stabilization_data->invalidateAssemblyCache();
}

void
KokkosSCZMInterfaceKernelSmallStrain::jacobianSetup()
{
  ShiftedInterfaceKernel::jacobianSetup();
  if (_stabilization_data)
    _stabilization_data->invalidateAssemblyCache();
}

void
KokkosSCZMInterfaceKernelSmallStrain::computeResidual()
{
  ensureSurrogateInterfaceMappingReady();
  if (_stabilization_data)
    _stabilization_data->ensurePrepared(_fe_problem.currentlyComputingResidualAndJacobian());
  _residual_dispatcher.reset();
  InterfaceKernel::computeResidual();
}

void
KokkosSCZMInterfaceKernelSmallStrain::computeJacobian()
{
  ensureSurrogateInterfaceMappingReady();
  if (_stabilization_data)
    _stabilization_data->ensurePrepared(true);
  _jacobian_dispatcher.reset();
  _offdiag_jacobian_dispatcher.reset();
  InterfaceKernel::computeJacobian();
}
