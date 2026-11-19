//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#ifdef NEML2_ENABLED

// MOOSE includes
#include "TorchSmallStrain.h"
#include "TorchMacroGradient.h"
#include "TorchFEMUtils.h"

registerMooseObject("SolidMechanicsApp", TorchSmallStrain);

InputParameters
TorchSmallStrain::validParams()
{
  InputParameters params = TorchPreKernel::validParams();
  params.addClassDescription(
      "This user object calculates the small strain from displacement gradients. "
      "It requires 1 to 3 displacement variables, which are used to compute the strain tensor.");
  params.addRequiredParam<std::vector<NonlinearVariableName>>(
      "displacements", "The displacements to use to calculate the strain.");
  params.addParam<bool>(
      "stabilize_strain",
      false,
      "If true, replace the trace of the displacement gradient by its element average (the "
      "small-kinematics total-mode F-bar correction of the host ComputeLagrangianStrain).");
  params.addCoupledVar("macro_gradient",
                       "Homogenization macro gradient scalar variable added to the displacement "
                       "gradient after the volumetric correction.");
  params.addParam<MultiMooseEnum>(
      "constraint_types",
      Homogenization::constraintType,
      "Homogenization constraint types, in column-major order, which map the macro gradient "
      "components to tensor components. Required with macro_gradient.");
  return params;
}

TorchSmallStrain::TorchSmallStrain(const InputParameters & parameters)
  : TorchPreKernel(parameters),
    _stabilize_strain(getParam<bool>("stabilize_strain")),
    _macro_gradient(isCoupledScalar("macro_gradient") ? getScalarVar("macro_gradient", 0)
                                                      : nullptr),
    _macro_gradient_indices(_macro_gradient ? Homogenization::constraintIndices(
                                                  getParam<MultiMooseEnum>("constraint_types"))
                                            : std::vector<std::pair<unsigned int, unsigned int>>())
{
  if (_macro_gradient && _macro_gradient->order() != _macro_gradient_indices.size())
    paramError("constraint_types",
               "The number of constraints must equal the order of the macro gradient variable.");

  auto disp_vars = getParam<std::vector<NonlinearVariableName>>("displacements");
  if (disp_vars.size() < 1 || disp_vars.size() > 3)
    mooseError("TorchSmallStrain requires 1 to 3 displacement variables, got ", disp_vars.size());

  _grad_disp_x = &_fe.getGradient(disp_vars[0]);
  _grad_disp_y = disp_vars.size() >= 2 ? &_fe.getGradient(disp_vars[1]) : nullptr;
  _grad_disp_z = disp_vars.size() >= 3 ? &_fe.getGradient(disp_vars[2]) : nullptr;
}

void
TorchSmallStrain::forward()
{
  // gradient of displacements, each (nelem, nqp, 3)
  const auto & dux = *_grad_disp_x;
  auto duy = _grad_disp_y ? *_grad_disp_y : at::zeros_like(dux);
  auto duz = _grad_disp_z ? *_grad_disp_z : at::zeros_like(dux);

  // displacement gradient du[..., i, j] = d u_i / d x_j, shape (nelem, nqp, 3, 3)
  auto du = at::stack({dux, duy, duz}, -2);

  // Additive F-bar: du += (tr(avg(du)) - tr(du)) / 3 * I, with the element average weighted by
  // TorchAssembly's JxWxT, as in the host ComputeLagrangianStrainBase with small kinematics
  if (_stabilize_strain)
  {
    const auto & JxWxT = _assembly.JxWxT();              // (nelem, nqp)
    const auto tr = at::diagonal(du, 0, -2, -1).sum(-1); // (nelem, nqp)
    const auto tr_avg = (tr * JxWxT).sum(1, true) / JxWxT.sum(1, true);
    du = du + ((tr_avg - tr) / 3).unsqueeze(-1).unsqueeze(-1) * at::eye(3, dux.options());
  }

  // The macro gradient is added after the volumetric correction
  if (_macro_gradient)
    du = du +
         Homogenization::macroGradient(*_macro_gradient, _macro_gradient_indices, dux.options());

  // strain = 0.5 * (grad_u + grad_u^T), stored in 6-component Mandel form
  _output = TorchFEM::fullToMandel(0.5 * (du + du.transpose(-2, -1)));
}

#endif // NEML2_ENABLED
