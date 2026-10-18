//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosADSCZMInterfaceKernelSmallStrain.h"

registerKokkosADInterfaceKernel("ShiftedBoundaryMethodApp", KokkosADSCZMInterfaceKernelSmallStrain);

InputParameters
KokkosADSCZMInterfaceKernelSmallStrain::validParams()
{
  auto params = ADShiftedInterfaceKernel::validParams();
  params.addRequiredParam<unsigned int>("component",
                                        "The displacement component assembled by this kernel.");
  params.addRequiredCoupledVar("displacements", "The displacement variables.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addParam<MaterialPropertyName>(
      "traction_global", "traction_global", "Global AD traction material property.");
  params.addParam<bool>(
      "no_shifted", false, "Disable shifted terms and assemble the plain interface residual.");
  params.addParam<bool>("directional_correction", true, "Add the directional correction terms.");
  params.addClassDescription(
      "AD Kokkos shifted cohesive-zone interface kernel for small-strain displacements.");
  return params;
}

KokkosADSCZMInterfaceKernelSmallStrain::KokkosADSCZMInterfaceKernelSmallStrain(
    const InputParameters & parameters)
  : ADShiftedInterfaceKernel(parameters),
    _component(getParam<unsigned int>("component")),
    _shifted(!getParam<bool>("no_shifted")),
    _traction_global(getKokkosMaterialPropertyByName<Moose::Kokkos::ADReal, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("traction_global")))
{
  const auto ndisp = coupledComponents("displacements");
  if (ndisp != _mesh.dimension())
    paramError("displacements", "Number of displacements must match problem dimension.");
  if (ndisp < 1 || ndisp > 3)
    paramError("displacements", "One, two, or three displacement variables are required.");
  if (_component >= ndisp)
    paramError("component", "Component must be smaller than the number of displacements.");
  if (getParam<bool>("directional_correction") && _shifted)
    paramError("directional_correction",
               "AD Kokkos SCZM directional correction requires an AD bulk-stress material chain, "
               "which is not implemented yet. Set directional_correction = false.");
}
