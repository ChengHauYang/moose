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
      "deformation_gradient", "deformation_gradient", "Name of the deformation gradient property.");
  return params;
}

KokkosComputeDeformationGradient::KokkosComputeDeformationGradient(
    const InputParameters & parameters)
  : Material(parameters),
    _ndisp(coupledComponents("displacements")),
    _grad_displacements(kokkosCoupledGradients("displacements")),
    _deformation_gradient(declareKokkosProperty<Real, 2>(
        getParam<MaterialPropertyName>("deformation_gradient"), {3, 3}))
{
  if (_ndisp != _mesh.dimension())
    paramError("displacements",
               "The number of displacement variables must match the mesh dimension.");
}
