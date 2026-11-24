//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosComputeStVenantKirchhoffStress.h"

registerKokkosMaterial("SolidMechanicsApp", KokkosComputeStVenantKirchhoffStress);

InputParameters
KokkosComputeStVenantKirchhoffStress::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription("Computes the isotropic St. Venant-Kirchhoff PK2 stress and its "
                             "deformation gradient derivative as Kokkos material properties.");
  params.addRequiredRangeCheckedParam<Real>(
      "youngs_modulus", "youngs_modulus > 0", "Young's modulus.");
  params.addRequiredRangeCheckedParam<Real>(
      "poissons_ratio", "poissons_ratio > -1 & poissons_ratio < 0.5", "Poisson's ratio.");
  params.addParam<MaterialPropertyName>(
      "deformation_gradient", "deformation_gradient", "Name of the deformation gradient.");
  params.addParam<MaterialPropertyName>("pk2_stress", "pk2_stress", "Name of the PK2 stress.");
  params.addParam<MaterialPropertyName>(
      "dpk2_dF", "dpk2_dF", "Name of the PK2 stress derivative with respect to F.");
  return params;
}

KokkosComputeStVenantKirchhoffStress::KokkosComputeStVenantKirchhoffStress(
    const InputParameters & parameters)
  : Material(parameters),
    _lambda(getParam<Real>("youngs_modulus") * getParam<Real>("poissons_ratio") /
            ((1 + getParam<Real>("poissons_ratio")) * (1 - 2 * getParam<Real>("poissons_ratio")))),
    _mu(getParam<Real>("youngs_modulus") / (2 * (1 + getParam<Real>("poissons_ratio")))),
    _deformation_gradient(getKokkosMaterialProperty<Real, 2>("deformation_gradient")),
    _pk2_stress(declareKokkosProperty<Real, 2>("pk2_stress", {3, 3})),
    _dpk2_dF(declareKokkosProperty<Real, 4>("dpk2_dF", {3, 3, 3, 3}))
{
}
