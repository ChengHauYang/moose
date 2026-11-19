//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosComputeIsotropicLinearElasticStress.h"

registerKokkosMaterial("SolidMechanicsApp", KokkosComputeIsotropicLinearElasticStress);

InputParameters
KokkosComputeIsotropicLinearElasticStress::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription("Computes the isotropic linear elastic small stress and its strain "
                             "derivative from a deformation gradient as Kokkos material "
                             "properties.");
  params.addRequiredRangeCheckedParam<Real>(
      "youngs_modulus", "youngs_modulus > 0", "Young's modulus.");
  params.addRequiredRangeCheckedParam<Real>(
      "poissons_ratio", "poissons_ratio > -1 & poissons_ratio < 0.5", "Poisson's ratio.");
  params.addParam<MaterialPropertyName>(
      "deformation_gradient", "deformation_gradient", "Name of the deformation gradient.");
  params.addParam<MaterialPropertyName>("stress", "stress", "Name of the stress material property.");
  params.addParam<MaterialPropertyName>(
      "tangent", "Jacobian_mult", "Name of the constitutive tangent material property.");
  return params;
}

KokkosComputeIsotropicLinearElasticStress::KokkosComputeIsotropicLinearElasticStress(
    const InputParameters & parameters)
  : Material(parameters),
    _lambda(getParam<Real>("youngs_modulus") * getParam<Real>("poissons_ratio") /
            ((1 + getParam<Real>("poissons_ratio")) * (1 - 2 * getParam<Real>("poissons_ratio")))),
    _mu(getParam<Real>("youngs_modulus") / (2 * (1 + getParam<Real>("poissons_ratio")))),
    _deformation_gradient(getKokkosMaterialProperty<Real, 2>("deformation_gradient")),
    _stress(declareKokkosProperty<Real, 2>("stress", {3, 3})),
    _tangent(declareKokkosProperty<Real, 4>("tangent", {3, 3, 3, 3}))
{
}
