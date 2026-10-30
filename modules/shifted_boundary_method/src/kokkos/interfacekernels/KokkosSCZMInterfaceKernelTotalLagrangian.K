//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSCZMInterfaceKernelTotalLagrangian.h"

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
  params.addParam<bool>(
      "volumetric_locking_correction",
      false,
      "Use the total-mode F-bar PK1 stress P(U, B) in the directional correction. The 'tangent' "
      "property is then d(P)/d(U) at fixed B, and the Jacobian adds the local and element-volume "
      "F-bar terms through B = (det(M) / det(U))^(1/3) U.");
  params.addParam<MaterialPropertyName>("stress", "pk1_stress", "PK1 stress material property.");
  params.addParam<MaterialPropertyName>(
      "tangent", "pk1_jacobian", "Constitutive tangent material property (d(PK1)/d(F)).");
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
  params.addParam<MooseEnum>("tangent_definition",
                             MooseEnum("auto stress_wrt_strain pk1_wrt_deformation_gradient",
                                       "pk1_wrt_deformation_gradient"),
                             "Mathematical definition of the tangent material property.");
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

  _disp_vars.create(_ndisp);
  for (unsigned int i = 0; i < _ndisp; ++i)
    _disp_vars[i] = coupled("displacements", i);
  _disp_vars.copyToDevice();

  if (_shifted && _directional_correction)
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
      if (_ndisp != 3)
        paramError("volumetric_locking_correction",
                   "The total-Lagrangian F-bar correction requires three-dimensional "
                   "displacements.");
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
