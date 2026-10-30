//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosComputeLagrangianStressCustomPK2.h"
#include "FEProblemBase.h"

registerKokkosMaterial("SolidMechanicsApp", KokkosComputeLagrangianStressCustomPK2);
registerKokkosInterfaceMaterial("SolidMechanicsApp",
                                KokkosInterfaceComputeLagrangianStressCustomPK2);
registerKokkosInterfaceMaterial("SolidMechanicsApp",
                                KokkosNeighborComputeLagrangianStressCustomPK2);

template <bool interface, bool neighbor>
InputParameters
KokkosComputeLagrangianStressCustomPK2Templ<interface, neighbor>::validParams()
{
  auto params = Base::validParams();
  params.addClassDescription(
      "Converts a full PK2 stress and its deformation-gradient derivative to PK1 stress and its "
      "displacement-gradient derivative. Optionally applies the total-mode F-bar Jacobian chain "
      "to match the host `ComputeLagrangianStressCustomPK2` when the strain calculator has "
      "`stabilize_strain = true`.");
  params.addRequiredCoupledVar("displacements", "Displacement variables in Cartesian order.");
  params.template addRequiredParam<MaterialPropertyName>("custom_pk2_stress",
                                                         "Name of the full PK2 stress.");
  params.template addRequiredParam<MaterialPropertyName>(
      "custom_pk2_jacobian", "Name of the full PK2 derivative with respect to F.");
  params.template addParam<MaterialPropertyName>(
      "pk1_stress", "pk1_stress", "Name of the PK1 stress.");
  params.template addParam<MaterialPropertyName>(
      "dpk1_d_grad_u", "dpk1_d_grad_u", "Name of the PK1 displacement-gradient derivative.");
  params.template addParam<bool>(
      "stabilize_strain",
      false,
      "If true, chain the total-mode F-bar dF_stab/dF_ust into the local PK1 Jacobian and "
      "publish A(qp) and gamma/3 for the non-local Jacobian assembled by the "
      "KokkosTotalLagrangianStressDivergence kernel. Interface conversion does not support F-bar.");
  params.template addParam<MaterialPropertyName>(
      "F_avg", "F_avg", "Name of the element-averaged deformation gradient (F-bar input).");
  params.template addParam<MaterialPropertyName>(
      "F_avg_invT",
      "F_avg_invT",
      "Name of the inverse-transpose element-averaged deformation gradient (declared for pairing "
      "with the kernel, not read here).");
  params.template addParam<MaterialPropertyName>(
      "fbar_A",
      "fbar_A",
      "Name of the F-bar A(qp) rank-two tensor published for the kernel's non-local Jacobian.");
  params.template addParam<MaterialPropertyName>(
      "fbar_gamma_over_3",
      "fbar_gamma_over_3",
      "Name of the F-bar gamma/3 scalar published for the kernel's non-local Jacobian.");

  if constexpr (neighbor)
    params.template set<Moose::MaterialDataType>("_material_data_type") =
        Moose::NEIGHBOR_MATERIAL_DATA;

  return params;
}

template <bool interface, bool neighbor>
KokkosComputeLagrangianStressCustomPK2Templ<interface, neighbor>::
    KokkosComputeLagrangianStressCustomPK2Templ(const InputParameters & parameters)
  : Base(parameters),
    _ndisp(this->coupledComponents("displacements")),
    _grad_displacements(this->kokkosCoupledGradients("displacements")),
    _pk2_stress(this->template getKokkosMaterialProperty<Real, 2>("custom_pk2_stress")),
    _dpk2_dF(this->template getKokkosMaterialProperty<Real, 4>("custom_pk2_jacobian")),
    _pk1_stress(this->template declareKokkosProperty<Real, 2>("pk1_stress", {3, 3})),
    _dpk1_d_grad_u(this->template declareKokkosProperty<Real, 4>("dpk1_d_grad_u", {3, 3, 3, 3})),
    _need_jacobian(false),
    _need_jacobian_device(_need_jacobian),
    _stabilize_strain(this->template getParam<bool>("stabilize_strain")),
    _stabilize_strain_device(_stabilize_strain),
    _F_avg(_stabilize_strain ? this->template getKokkosMaterialProperty<Real, 2>("F_avg")
                             : Moose::Kokkos::MaterialProperty<Real, 2>()),
    _F_avg_invT(_stabilize_strain ? this->template getKokkosMaterialProperty<Real, 2>("F_avg_invT")
                                  : Moose::Kokkos::MaterialProperty<Real, 2>()),
    _A(_stabilize_strain ? this->template declareKokkosProperty<Real, 2>("fbar_A", {3, 3})
                         : Moose::Kokkos::MaterialProperty<Real, 2>()),
    _gamma_over_3(_stabilize_strain
                      ? this->template declareKokkosProperty<Real, 0>("fbar_gamma_over_3")
                      : Moose::Kokkos::MaterialProperty<Real, 0>())
{
  if (this->_mesh.dimension() != 3 || _ndisp != 3)
    this->paramError("displacements",
                     "Exactly three displacement variables on a 3D mesh are required.");
  if constexpr (interface)
    if (_stabilize_strain)
      this->paramError("stabilize_strain",
                       "F-bar stabilization is not supported for interface PK2-to-PK1 conversion.");
}

template <bool interface, bool neighbor>
void
KokkosComputeLagrangianStressCustomPK2Templ<interface, neighbor>::computeProperties()
{
  _need_jacobian = this->_fe_problem.currentlyComputingJacobian() ||
                   this->_fe_problem.currentlyComputingResidualAndJacobian();
  Base::computeProperties();
}

template class KokkosComputeLagrangianStressCustomPK2Templ<false, false>;
template class KokkosComputeLagrangianStressCustomPK2Templ<true, false>;
template class KokkosComputeLagrangianStressCustomPK2Templ<true, true>;
