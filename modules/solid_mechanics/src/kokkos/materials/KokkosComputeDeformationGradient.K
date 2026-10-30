//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosComputeDeformationGradient.h"

registerKokkosMaterial("SolidMechanicsApp", KokkosComputeDeformationGradient);

InputParameters
KokkosComputeDeformationGradient::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription(
      "Computes the total deformation gradient as a Kokkos rank-two material property.");
  params.addRequiredCoupledVar("displacements", "Displacement variables in Cartesian order.");
  params.addParam<MaterialPropertyName>(
      "deformation_gradient",
      "deformation_gradient",
      "Name of the deformation gradient property. With stabilize_strain, this is the total-mode "
      "F-bar deformation gradient.");
  params.addParam<bool>(
      "stabilize_strain",
      false,
      "Also compute the total-mode F-bar deformation gradient from the average of the physical "
      "deformation gradient over the true reference volume of the owning element.");
  params.addParam<MaterialPropertyName>(
      "physical_deformation_gradient",
      "physical_deformation_gradient",
      "Name of the physical deformation gradient property used with stabilize_strain.");
  params.addParam<MaterialPropertyName>(
      "average_deformation_gradient",
      "average_deformation_gradient",
      "Name of the element-volume average of the physical deformation gradient used with "
      "stabilize_strain.");
  return params;
}

KokkosComputeDeformationGradient::KokkosComputeDeformationGradient(
    const InputParameters & parameters)
  : Material(parameters),
    _ndisp(coupledComponents("displacements")),
    _stabilize_strain(getParam<bool>("stabilize_strain")),
    _grad_displacements(kokkosCoupledGradients("displacements")),
    _physical_deformation_gradient(declareKokkosProperty<Real, 2>(
        getParam<MaterialPropertyName>(_stabilize_strain ? "physical_deformation_gradient"
                                                         : "deformation_gradient"),
        {3, 3}))
{
  if (_ndisp != _mesh.dimension())
    paramError("displacements",
               "The number of displacement variables must match the mesh dimension.");

  if (_stabilize_strain)
  {
    if (_ndisp != 3)
      paramError("stabilize_strain",
                 "F-bar stabilization requires three-dimensional displacements.");
    _stabilized_deformation_gradient = declareKokkosProperty<Real, 2>(
        getParam<MaterialPropertyName>("deformation_gradient"), {3, 3});
    _average_deformation_gradient = declareKokkosProperty<Real, 2>(
        getParam<MaterialPropertyName>("average_deformation_gradient"), {3, 3});
  }
  else
    for (const auto & param : {"physical_deformation_gradient", "average_deformation_gradient"})
      if (isParamSetByUser(param))
        paramError(param, "This parameter requires stabilize_strain = true.");
}
