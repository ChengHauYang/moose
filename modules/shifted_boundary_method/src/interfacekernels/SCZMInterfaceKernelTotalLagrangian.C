//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SCZMInterfaceKernelTotalLagrangian.h"
#include "StabilizationUtils.h"

namespace
{
RankTwoTensor
tlGradOpForCoord(const Moose::CoordinateSystemType coord_sys,
                 const unsigned int component,
                 const RealVectorValue & grad,
                 const Real value,
                 const Point & point)
{
  RankTwoTensor grad_op;

  switch (coord_sys)
  {
    case Moose::COORD_XYZ:
      for (const auto j : make_range(3))
        grad_op(component, j) = grad(j);
      break;

    case Moose::COORD_RZ:
      for (const auto j : make_range(2))
        grad_op(component, j) = grad(j);
      if (component == 0)
        grad_op(2, 2) = value / point(0);
      break;

    case Moose::COORD_RSPHERICAL:
      grad_op(0, 0) = grad(0);
      grad_op(1, 1) = value / point(0);
      grad_op(2, 2) = value / point(0);
      break;

    default:
      mooseError("Unsupported coordinate system for SCZMInterfaceKernelTotalLagrangian.");
  }

  return grad_op;
}
}

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
        "Whether to apply volumetric locking correction to the directional correction Jacobian.");
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
  }
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
    const unsigned int ivar, const unsigned int jvar, const Moose::DGJacobianType type) const
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
      const auto grad_op =
          tlGradOpForCoord(this->_coord_sys,
                           jvar,
                           grad_phi,
                           trial_on_element ? this->_phi[this->_j][this->_qp]
                                            : this->_phi_neighbor[this->_j][this->_qp],
                           this->_q_point[this->_qp]);
      const auto avg_grad_op = StabilizationUtils::elementAverage(
          [this, jvar, trial_on_element](unsigned int qp)
          {
            return tlGradOpForCoord(this->_coord_sys,
                                    jvar,
                                    trial_on_element ? this->_grad_phi[this->_j][qp]
                                                     : this->_grad_phi_neighbor[this->_j][qp],
                                    trial_on_element ? this->_phi[this->_j][qp]
                                                     : this->_phi_neighbor[this->_j][qp],
                                    this->_q_point[qp]);
          },
          this->_JxW,
          this->_coord);
      const Real volumetric_increment = (avg_grad_op.trace() - grad_op.trace()) / 3.0;
      for (const auto J : make_range(3))
        for (const auto L : make_range(3))
          jacobian += tangent(ivar, J, L, L) * normal_tangent(J) * test * volumetric_increment;
    }

    return jacobian;
  }
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
