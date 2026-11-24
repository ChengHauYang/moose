//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosComputeLagrangianStressCustomSmall.h"
#include "FEProblemBase.h"

registerKokkosMaterial("SolidMechanicsApp", KokkosComputeLagrangianStressCustomSmall);

InputParameters
KokkosComputeLagrangianStressCustomSmall::validParams()
{
  auto params = Material::validParams();
  params.addClassDescription(
      "Publishes the small-kinematics PK1 stress and its displacement-gradient derivative for the "
      "Kokkos total-Lagrangian kernel from a small stress and its strain derivative, optionally "
      "with the additive F-bar Jacobian chain.");
  params.addRequiredParam<MaterialPropertyName>("custom_small_stress",
                                                "Name of the small (Cauchy) stress.");
  params.addRequiredParam<MaterialPropertyName>(
      "custom_small_jacobian", "Name of the small stress derivative with respect to the strain.");
  params.addParam<MaterialPropertyName>("pk1_stress", "pk1_stress", "Name of the PK1 stress.");
  params.addParam<MaterialPropertyName>(
      "dpk1_d_grad_u", "dpk1_d_grad_u", "Name of the PK1 displacement-gradient derivative.");
  params.addParam<bool>(
      "stabilize_strain",
      false,
      "If true, chain the additive F-bar dF_stab/dF_ust into the PK1 Jacobian and publish A(qp) "
      "and gamma/3 for the non-local Jacobian of KokkosTotalLagrangianStressDivergence.");
  params.addParam<MaterialPropertyName>(
      "fbar_A",
      "fbar_A",
      "Name of the F-bar A(qp) rank-two tensor published for the kernel's non-local Jacobian.");
  params.addParam<MaterialPropertyName>(
      "fbar_gamma_over_3",
      "fbar_gamma_over_3",
      "Name of the F-bar gamma/3 scalar published for the kernel's non-local Jacobian.");
  params.addParam<MaterialPropertyName>(
      "dpk1_bypass_fbar",
      "dpk1_bypass_fbar",
      "Name of the PK1 tangent without the F-bar factor, published with stabilize_strain.");
  return params;
}

KokkosComputeLagrangianStressCustomSmall::KokkosComputeLagrangianStressCustomSmall(
    const InputParameters & parameters)
  : Material(parameters),
    _small_stress(getKokkosMaterialProperty<Real, 2>("custom_small_stress")),
    _small_jacobian(getKokkosMaterialProperty<Real, 4>("custom_small_jacobian")),
    _pk1_stress(declareKokkosProperty<Real, 2>("pk1_stress", {3, 3})),
    _dpk1_d_grad_u(declareKokkosProperty<Real, 4>("dpk1_d_grad_u", {3, 3, 3, 3})),
    _need_jacobian(false),
    _need_jacobian_device(_need_jacobian),
    _stabilize_strain(getParam<bool>("stabilize_strain")),
    _A(_stabilize_strain ? declareKokkosProperty<Real, 2>("fbar_A", {3, 3})
                         : Moose::Kokkos::MaterialProperty<Real, 2>()),
    _gamma_over_3(_stabilize_strain ? declareKokkosProperty<Real, 0>("fbar_gamma_over_3")
                                    : Moose::Kokkos::MaterialProperty<Real, 0>()),
    _dpk1_bypass_fbar(_stabilize_strain
                          ? declareKokkosProperty<Real, 4>("dpk1_bypass_fbar", {3, 3, 3, 3})
                          : Moose::Kokkos::MaterialProperty<Real, 4>())
{
}

void
KokkosComputeLagrangianStressCustomSmall::computeProperties()
{
  _need_jacobian = _fe_problem.currentlyComputingJacobian() ||
                   _fe_problem.currentlyComputingResidualAndJacobian();
  Material::computeProperties();
}
