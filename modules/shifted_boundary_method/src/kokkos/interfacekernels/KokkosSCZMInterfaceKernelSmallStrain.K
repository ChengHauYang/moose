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
  params.addParam<bool>(
      "directional_correction",
      true,
      "Add the directional correction terms. When enabled, the bulk stress and Lame constants are "
      "read from the face properties written by KokkosSCZMComputeLinearElasticBulkStress.");
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

  _disp_vars.create(_ndisp);
  for (unsigned int i = 0; i < _ndisp; ++i)
    _disp_vars[i] = coupled("displacements", i);
  _disp_vars.copyToDevice();

  // The correction bulk properties come from the user-declared
  // KokkosSCZMComputeLinearElasticBulkStress material, so they are retrieved only when the
  // correction is actually applied. Following the host, no_shifted suppresses the correction
  // entirely. Missing declarations are reported here by getKokkosMaterialPropertyByName.
  if (_shifted && _directional_correction)
  {
    const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                            ? getParam<std::string>("base_name") + "_"
                            : "";
    _sczm_bulk_stress = getKokkosMaterialPropertyByName<Real, 2>(prefix + "sczm_bulk_stress");
    _sczm_bulk_stress_neighbor =
        getKokkosMaterialPropertyByName<Real, 2>(prefix + "sczm_bulk_stress_neighbor");
    _sczm_bulk_lame = getKokkosMaterialPropertyByName<Real, 1>(prefix + "sczm_bulk_lame");
    _sczm_bulk_lame_neighbor =
        getKokkosMaterialPropertyByName<Real, 1>(prefix + "sczm_bulk_lame_neighbor");
  }
}
