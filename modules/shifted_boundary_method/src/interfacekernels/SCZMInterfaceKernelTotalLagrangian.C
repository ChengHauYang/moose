//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SCZMInterfaceKernelTotalLagrangian.h"
#include "MooseVariableFE.h"

registerMooseObject("ShiftedBoundaryMethodApp", SCZMInterfaceKernelTotalLagrangian);
registerMooseObject("ShiftedBoundaryMethodApp", ADSCZMInterfaceKernelTotalLagrangian);

template <bool is_ad>
InputParameters
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::validParams()
{
  InputParameters params = SCZMInterfaceKernelBaseTempl<is_ad>::validParams();
  params.set<std::string>("traction_global_name") = "PK1traction";
  params.addParam<bool>("directional_correction", true, "Add the directional correction terms.");
  params.addParam<MaterialPropertyName>(
      "stress", "pk1_stress", "Name of the PK1 stress tensor material property.");
  if constexpr (!is_ad)
  {
    params.addParam<MaterialPropertyName>(
        "tangent", "pk1_jacobian", "Name of the derivative of PK1 stress with respect to F.");
    params.addParam<MooseEnum>("tangent_definition",
                               MooseEnum("auto stress_wrt_strain pk1_wrt_deformation_gradient",
                                         "pk1_wrt_deformation_gradient"),
                               "Mathematical definition of the tangent material property.");
    params.addParam<bool>(
        "volumetric_locking_correction",
        false,
        "Use the total-mode F-bar PK1 stress P(U, B) in the directional correction. The 'tangent' "
        "property is then d(P)/d(U) at fixed B, and the Jacobian adds the local and element-volume "
        "F-bar terms through B = (det(M) / det(U))^(1/3) U.");
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
  }
  params.addClassDescription(
      "Assembles shifted cohesive traction using total-Lagrangian kinematics.");
  return params;
}

template <bool is_ad>
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::SCZMInterfaceKernelTotalLagrangianTempl(
    const InputParameters & parameters)
  : SCZMInterfaceKernelBaseTempl<is_ad>(parameters),
    _stress(parameters.get<bool>("directional_correction")
                ? &this->template getGenericMaterialPropertyByName<RankTwoTensor, is_ad>(
                      this->_base_name + parameters.get<MaterialPropertyName>("stress"))
                : nullptr),
    _stress_neighbor(
        parameters.get<bool>("directional_correction")
            ? &this->template getGenericNeighborMaterialPropertyByName<RankTwoTensor, is_ad>(
                  this->_base_name + parameters.get<MaterialPropertyName>("stress"))
            : nullptr),
    _dtraction_djump_global(nullptr),
    _dPK1traction_dF(nullptr),
    _Jacobian_mult(nullptr),
    _Jacobian_mult_neighbor(nullptr),
    _stabilized_tangent(nullptr),
    _stabilized_tangent_neighbor(nullptr),
    _physical_deformation_gradient(nullptr),
    _physical_deformation_gradient_neighbor(nullptr),
    _average_deformation_gradient(nullptr),
    _average_deformation_gradient_neighbor(nullptr),
    _avg_grad_phi_elem(nullptr),
    _avg_grad_phi_neighbor_elem(nullptr),
    _directional_correction(parameters.get<bool>("directional_correction")),
    _volumetric_locking_correction(false)
{
  if constexpr (!is_ad)
  {
    _disp_var.resize(this->_ndisp);
    _vars.resize(this->_ndisp);
    for (const auto i : make_range(this->_ndisp))
    {
      _disp_var[i] = this->coupled("displacements", i);
      _vars[i] = this->getVar("displacements", i);
    }

    _dtraction_djump_global = &this->template getMaterialPropertyByName<RankTwoTensor>(
        this->_base_name + "dtraction_djump_global");
    _dPK1traction_dF = &this->template getMaterialPropertyByName<RankThreeTensor>(
        this->_base_name + "dPK1traction_dF");
    if (this->_shifted && _directional_correction)
    {
      _Jacobian_mult = &this->template getMaterialPropertyByName<RankFourTensor>(
          this->_base_name + parameters.get<MaterialPropertyName>("tangent"));
      _Jacobian_mult_neighbor = &this->template getNeighborMaterialPropertyByName<RankFourTensor>(
          this->_base_name + parameters.get<MaterialPropertyName>("tangent"));
    }
    _volumetric_locking_correction = parameters.get<bool>("volumetric_locking_correction");

    if (_volumetric_locking_correction && this->_shifted && _directional_correction)
    {
      if (this->_ndisp != 3)
        this->paramError("volumetric_locking_correction",
                         "The total-Lagrangian F-bar correction requires three-dimensional "
                         "displacements.");
      if (parameters.get<MooseEnum>("tangent_definition") != "pk1_wrt_deformation_gradient")
        this->paramError("tangent_definition",
                         "The total-Lagrangian F-bar correction requires "
                         "'pk1_wrt_deformation_gradient'.");
      for (const auto i : make_range(this->_ndisp))
        if (_vars[i]->feType() != _vars[0]->feType())
          this->paramError("displacements",
                           "The total-Lagrangian F-bar correction requires all displacements to "
                           "share one finite element type.");

      const auto stabilized_tangent =
          this->_base_name + parameters.get<MaterialPropertyName>("stabilized_tangent");
      const auto physical =
          this->_base_name + parameters.get<MaterialPropertyName>("physical_deformation_gradient");
      const auto average =
          this->_base_name + parameters.get<MaterialPropertyName>("average_deformation_gradient");
      _stabilized_tangent =
          &this->template getMaterialPropertyByName<RankFourTensor>(stabilized_tangent);
      _stabilized_tangent_neighbor =
          &this->template getNeighborMaterialPropertyByName<RankFourTensor>(stabilized_tangent);
      _physical_deformation_gradient =
          &this->template getMaterialPropertyByName<RankTwoTensor>(physical);
      _physical_deformation_gradient_neighbor =
          &this->template getNeighborMaterialPropertyByName<RankTwoTensor>(physical);
      _average_deformation_gradient =
          &this->template getMaterialPropertyByName<RankTwoTensor>(average);
      _average_deformation_gradient_neighbor =
          &this->template getNeighborMaterialPropertyByName<RankTwoTensor>(average);
    }
  }
}

template <bool is_ad>
void
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::initialSetup()
{
  SCZMInterfaceKernelBaseTempl<is_ad>::initialSetup();

  if (_stabilized_tangent)
    for (const auto bid : this->boundaryIDs())
      for (const auto block : this->_mesh.getBoundaryConnectedBlocks(bid))
        if (this->_mesh.getCoordSystem(block) != Moose::COORD_XYZ)
          this->paramError("volumetric_locking_correction",
                           "The total-Lagrangian F-bar correction only supports Cartesian "
                           "coordinates.");
}

template <bool is_ad>
void
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::jacobianSetup()
{
  SCZMInterfaceKernelBaseTempl<is_ad>::jacobianSetup();

  // Element pointers may be reused after mesh changes, so the averages are rebuilt each assembly.
  _avg_grad_phi_elem = nullptr;
  _avg_grad_phi_neighbor_elem = nullptr;
}

template <bool is_ad>
GenericReal<is_ad>
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::computeQpResidual(Moose::DGResidualType type)
{
  if (!this->_shifted)
    return SCZMInterfaceKernelBaseTempl<is_ad>::computeQpResidual(type);

  const RealVectorValue true_normal(this->trueNormal());
  const Real true_normal_dot_surrogate_normal = true_normal * this->_normals[this->_qp];

  auto residual = SCZMInterfaceKernelBaseTempl<is_ad>::computeQpResidual(type);
  // This maps true reference-interface area to surrogate area; it is separate from the Nanson
  // mapping already included in PK1traction.
  residual *= true_normal_dot_surrogate_normal;

  if (_directional_correction)
  {
    const auto stress = (*_stress)[this->_qp].row(this->_component);
    const auto stress_neighbor = (*_stress_neighbor)[this->_qp].row(this->_component);
    const auto normal_tangent =
        this->_normals[this->_qp] - true_normal_dot_surrogate_normal * true_normal;

    if (type == Moose::Element)
      residual -= this->_test[this->_i][this->_qp] * (stress * normal_tangent);
    else if (type == Moose::Neighbor)
      residual += this->_test_neighbor[this->_i][this->_qp] * (stress_neighbor * normal_tangent);
  }

  return residual;
}

template <bool is_ad>
Real
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::computeDResidualDDisplacement(
    const unsigned int component_j, const Moose::DGJacobianType type) const
{
  if constexpr (is_ad)
    return 0;
  else
  {
    Real derivative = (*_dtraction_djump_global)[this->_qp](this->_component, component_j);
    const auto d = this->_shifted ? this->surrogateDistance() : RealVectorValue();
    const bool trial_on_neighbor =
        type == Moose::ElementNeighbor || type == Moose::NeighborNeighbor;
    const auto phi = trial_on_neighbor
                         ? this->_vars[component_j]->phiFaceNeighbor()[this->_j][this->_qp]
                         : this->_vars[component_j]->phiFace()[this->_j][this->_qp];
    const auto & grad_phi =
        trial_on_neighbor ? this->_vars[component_j]->gradPhiFaceNeighbor()[this->_j][this->_qp]
                          : this->_vars[component_j]->gradPhiFace()[this->_j][this->_qp];
    derivative *= this->_shifted ? phi + grad_phi * d : phi;

    const bool same_side = type == Moose::ElementElement || type == Moose::NeighborNeighbor;
    derivative *= same_side ? 1.0 : -1.0;

    if (this->_shifted)
      derivative *= RealVectorValue(this->trueNormal()) * this->_normals[this->_qp];

    const bool residual_on_element =
        type == Moose::ElementElement || type == Moose::ElementNeighbor;
    return derivative * (residual_on_element ? this->_test[this->_i][this->_qp]
                                             : this->_test_neighbor[this->_i][this->_qp]);
  }
}

template <bool is_ad>
Real
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::computeDResidualDDeformationGradient(
    const unsigned int component_j, const Moose::DGJacobianType type) const
{
  if constexpr (is_ad)
    return 0;
  else
  {
    const bool trial_on_neighbor =
        type == Moose::ElementNeighbor || type == Moose::NeighborNeighbor;
    const auto & grad_phi =
        trial_on_neighbor ? this->_vars[component_j]->gradPhiFaceNeighbor()[this->_j][this->_qp]
                          : this->_vars[component_j]->gradPhiFace()[this->_j][this->_qp];

    // TODO: Include the displacement-Hessian contribution if F is Taylor-shifted to the true
    // interface.
    Real derivative = 0;
    for (const auto j : make_range(3))
      derivative +=
          (*_dPK1traction_dF)[this->_qp](this->_component, component_j, j) * 0.5 * grad_phi(j);

    if (this->_shifted)
      derivative *= RealVectorValue(this->trueNormal()) * this->_normals[this->_qp];

    const bool residual_on_element =
        type == Moose::ElementElement || type == Moose::ElementNeighbor;
    return derivative * (residual_on_element ? -this->_test[this->_i][this->_qp]
                                             : this->_test_neighbor[this->_i][this->_qp]);
  }
}

template <bool is_ad>
Real
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::calculateDirectionalCorrectionJacobian(
    const unsigned int ivar, const unsigned int jvar, const Moose::DGJacobianType type)
{
  if constexpr (is_ad)
    return 0;
  else
  {
    const auto true_normal = this->_shifted ? RealVectorValue(this->trueNormal())
                                            : RealVectorValue(this->_normals[this->_qp]);
    const auto normal_tangent =
        this->_normals[this->_qp] - (true_normal * this->_normals[this->_qp]) * true_normal;
    const bool trial_on_element = type == Moose::ElementElement || type == Moose::NeighborElement;
    const auto & grad_phi = trial_on_element ? this->_grad_phi[this->_j][this->_qp]
                                             : this->_grad_phi_neighbor[this->_j][this->_qp];
    const auto test = (type == Moose::ElementElement || type == Moose::ElementNeighbor)
                          ? this->_test[this->_i][this->_qp]
                          : this->_test_neighbor[this->_i][this->_qp];
    const auto & tangent = (type == Moose::ElementElement || type == Moose::ElementNeighbor)
                               ? (*_Jacobian_mult)[this->_qp]
                               : (*_Jacobian_mult_neighbor)[this->_qp];

    Real jacobian = 0;
    for (const auto J : make_range(3))
      for (const auto L : make_range(3))
        jacobian += tangent(ivar, J, jvar, L) * grad_phi(L) * normal_tangent(J) * test;

    if (_volumetric_locking_correction)
    {
      // The F-bar terms only couple a side's stress to the displacements of the same element, so
      // this is reached for ElementElement and NeighborNeighbor only.
      mooseAssert(type == Moose::ElementElement || type == Moose::NeighborNeighbor,
                  "The F-bar directional correction only couples same-side displacements.");
      const bool on_element = type == Moose::ElementElement;
      const auto & avg_grad_phi =
          on_element ? volumeAverageGradPhi(this->_current_elem, _avg_grad_phi_elem, _avg_grad_phi)
                     : volumeAverageGradPhi(this->_neighbor_elem,
                                            _avg_grad_phi_neighbor_elem,
                                            _avg_grad_phi_neighbor);
      jacobian += test * fbarDirectionalCorrection(
                             ivar,
                             jvar,
                             on_element ? (*_stabilized_tangent)[this->_qp]
                                        : (*_stabilized_tangent_neighbor)[this->_qp],
                             on_element ? (*_physical_deformation_gradient)[this->_qp]
                                        : (*_physical_deformation_gradient_neighbor)[this->_qp],
                             on_element ? (*_average_deformation_gradient)[this->_qp]
                                        : (*_average_deformation_gradient_neighbor)[this->_qp],
                             normal_tangent,
                             grad_phi,
                             avg_grad_phi[this->_j]);
    }

    return jacobian;
  }
}

template <bool is_ad>
Real
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::fbarDirectionalCorrection(
    const unsigned int ivar,
    const unsigned int jvar,
    const RankFourTensor & stabilized_tangent,
    const RankTwoTensor & U,
    const RankTwoTensor & M,
    const RealVectorValue & normal_tangent,
    const RealVectorValue & grad_phi,
    const RealVectorValue & avg_grad_phi) const
{
  const Real gamma = std::cbrt(M.det() / U.det());
  const auto U_inv = U.inverse();
  const auto M_inv = M.inverse();

  // For the trial direction e_c (x) grad(phi_j) with c = jvar, U^{-T} : dU and M^{-T} : dM reduce
  // to the c-th column of the inverses contracted with the local and element-volume averaged trial
  // gradients.
  Real U_invT_dU = 0;
  Real M_invT_dM = 0;
  for (const auto L : make_range(3))
  {
    U_invT_dU += U_inv(L, jvar) * grad_phi(L);
    M_invT_dM += M_inv(L, jvar) * avg_grad_phi(L);
  }

  // With B = gamma U and dgamma = gamma / 3 (M^{-T} : dM - U^{-T} : dU), the B path of dP is
  // C_B : dB = gamma C_B : dU + Q dgamma, where Q = C_B : U.
  Real correction = 0;
  for (const auto J : make_range(3))
  {
    Real Q = 0;
    Real stabilized_tangent_dU = 0;
    for (const auto L : make_range(3))
    {
      stabilized_tangent_dU += stabilized_tangent(ivar, J, jvar, L) * grad_phi(L);
      for (const auto k : make_range(3))
        Q += stabilized_tangent(ivar, J, k, L) * U(k, L);
    }
    correction += normal_tangent(J) *
                  (gamma * stabilized_tangent_dU + gamma / 3.0 * Q * (M_invT_dM - U_invT_dU));
  }

  return correction;
}

template <bool is_ad>
const std::vector<RealVectorValue> &
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::volumeAverageGradPhi(
    const Elem * const elem, const Elem *& cached_elem, std::vector<RealVectorValue> & cache)
{
  if (elem == cached_elem)
    return cache;

  // Reuse the volume rule of the current assembly context, matching the rule used by
  // ComputeDeformationGradient for M. Elements on both sides of the interface must therefore share
  // one volume quadrature rule.
  if (!_volume_fe)
  {
    const auto & volume_rule = *this->_assembly.qRule();
    _volume_qrule =
        libMesh::QBase::build(volume_rule.type(), this->_mesh.dimension(), volume_rule.get_order());
    _volume_fe = libMesh::FEBase::build(this->_mesh.dimension(), _vars[0]->feType());
    _volume_fe->attach_quadrature_rule(_volume_qrule.get());
    _volume_fe->get_dphi();
    _volume_fe->get_JxW();
  }

  _volume_fe->reinit(elem);
  const auto & dphi = _volume_fe->get_dphi();
  const auto & JxW = _volume_fe->get_JxW();

  Real measure = 0;
  for (const auto qp : index_range(JxW))
    measure += JxW[qp];

  cache.assign(dphi.size(), RealVectorValue());
  for (const auto j : index_range(dphi))
    for (const auto qp : index_range(JxW))
      cache[j] += JxW[qp] * dphi[j][qp] / measure;

  cached_elem = elem;
  return cache;
}

template <bool is_ad>
Real
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::computeQpJacobian(Moose::DGJacobianType type)
{
  if constexpr (is_ad)
    return 0;
  else
  {
    auto jacobian = computeDResidualDDisplacement(this->_component, type) +
                    computeDResidualDDeformationGradient(this->_component, type);
    if (this->_shifted && _directional_correction)
    {
      if (type == Moose::ElementElement)
        jacobian -=
            calculateDirectionalCorrectionJacobian(this->_component, this->_component, type);
      else if (type == Moose::NeighborNeighbor)
        jacobian +=
            calculateDirectionalCorrectionJacobian(this->_component, this->_component, type);
    }
    return jacobian;
  }
}

template <bool is_ad>
Real
SCZMInterfaceKernelTotalLagrangianTempl<is_ad>::computeQpOffDiagJacobian(Moose::DGJacobianType type,
                                                                         unsigned int jvar)
{
  if constexpr (is_ad)
    return 0;
  else
  {
    if (this->getJvarMap()[jvar] < 0)
      return 0;

    Real jacobian = 0;
    for (const auto component_j : make_range(this->_ndisp))
      if (jvar == _disp_var[component_j])
      {
        jacobian = computeDResidualDDisplacement(component_j, type) +
                   computeDResidualDDeformationGradient(component_j, type);
        if (this->_shifted && _directional_correction)
        {
          if (type == Moose::ElementElement)
            jacobian -= calculateDirectionalCorrectionJacobian(this->_component, component_j, type);
          else if (type == Moose::NeighborNeighbor)
            jacobian += calculateDirectionalCorrectionJacobian(this->_component, component_j, type);
        }
      }
    return jacobian;
  }
}

template class SCZMInterfaceKernelTotalLagrangianTempl<false>;
template class SCZMInterfaceKernelTotalLagrangianTempl<true>;
