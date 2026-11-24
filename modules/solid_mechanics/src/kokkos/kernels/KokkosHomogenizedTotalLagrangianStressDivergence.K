//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosHomogenizedTotalLagrangianStressDivergence.h"
#include "FEProblemBase.h"

registerKokkosResidualObject("SolidMechanicsApp",
                             KokkosHomogenizedTotalLagrangianStressDivergence);

InputParameters
KokkosHomogenizedTotalLagrangianStressDivergence::validParams()
{
  InputParameters params = HomogenizationInterface<ScalarBase>::validParams();
  params.addClassDescription("Kokkos total-Lagrangian stress equilibrium kernel with the "
                             "homogenization constraint residual and Jacobian terms.");
  params.renameCoupledVar("scalar_variable", "macro_var", "Scalar field with the macro gradient");
  params.addParam<bool>(
      "off_diagonal_jacobian", true, "Include the off diagonal parts of the constraint Jacobian");
  params.addParam<MaterialPropertyName>(
      "deformation_gradient",
      "deformation_gradient",
      "Name of the stabilized deformation gradient including the macro gradient, used by the "
      "strain constraints.");
  params.addParam<MaterialPropertyName>(
      "dpk1_bypass_fbar",
      "dpk1_bypass_fbar",
      "Name of the PK1 tangent without the F-bar dF_stab/dF_ust factor, used with "
      "stabilize_strain = true.");
  return params;
}

KokkosHomogenizedTotalLagrangianStressDivergence::KokkosHomogenizedTotalLagrangianStressDivergence(
    const InputParameters & parameters)
  : HomogenizationInterface<ScalarBase>(parameters),
    _use_off_diagonal(getParam<bool>("off_diagonal_jacobian")),
    _cmap_i(cmap().size()),
    _cmap_j(cmap().size()),
    _cmap_type(cmap().size()),
    _targets(cmap().size()),
    _dpk1_bypass_fbar(_stabilize_strain ? getKokkosMaterialProperty<Real, 4>("dpk1_bypass_fbar")
                                        : _dpk1_d_grad_u),
    _grad_disp(_stabilize_strain ? kokkosCoupledGradients("displacements")
                                 : Moose::Kokkos::VariableGradient())
{
  if (_k_order != cmap().size())
    paramError("macro_var",
               "The order of the macro gradient variable must equal the number of constraints.");

  bool has_strain_constraint = false;
  unsigned int h = 0;
  for (const auto & [indices, constraint] : cmap())
  {
    std::tie(_cmap_i[h], _cmap_j[h]) = indices;
    _cmap_type[h] = constraint.first;
    has_strain_constraint |= constraint.first == Homogenization::ConstraintType::Strain;
    ++h;
  }
  _cmap_i.copyToDevice();
  _cmap_j.copyToDevice();
  _cmap_type.copyToDevice();

  if (has_strain_constraint)
    _F = getKokkosMaterialProperty<Real, 2>("deformation_gradient");
}

void
KokkosHomogenizedTotalLagrangianStressDivergence::computeResidual()
{
  unsigned int h = 0;
  for (const auto & [indices, constraint] : cmap())
    _targets[h++] = constraint.second->value(_fe_problem.time(), Point());
  _targets.copyToDevice();

  ScalarBase::computeResidual();
}

void
KokkosHomogenizedTotalLagrangianStressDivergence::computeScalarResidual()
{
  // Only one kernel instance assembles the scalar residual
  if (_component == 0)
    ScalarBase::computeScalarResidual();
}

void
KokkosHomogenizedTotalLagrangianStressDivergence::computeScalarJacobian()
{
  // Only one kernel instance assembles the scalar-scalar Jacobian
  if (_component == 0)
    ScalarBase::computeScalarJacobian();
}

void
KokkosHomogenizedTotalLagrangianStressDivergence::computeScalarOffDiagJacobian()
{
  if (_use_off_diagonal)
    ScalarBase::computeScalarOffDiagJacobian();
}
