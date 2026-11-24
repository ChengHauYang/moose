//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosScalarLMKernel.h"

registerKokkosResidualObject("MooseTestApp", KokkosScalarLMKernel);

InputParameters
KokkosScalarLMKernel::validParams()
{
  InputParameters params = KernelScalarBase<Kernel>::validParams();
  params.addClassDescription("Enforces the integral of the variable with a Lagrange multiplier "
                             "scalar variable.");
  params.renameCoupledVar("scalar_variable", "kappa", "Primary coupled scalar variable");
  params.addRequiredParam<PostprocessorName>(
      "pp_name", "Name of the Postprocessor containing the volume of the domain.");
  params.addRequiredParam<Real>(
      "value", "Given (constant) which we want the integral of the solution variable to match.");
  params.addParam<Real>(
      "penalty", 0, "Coefficient of the scalar variable added to the scalar residual.");
  return params;
}

KokkosScalarLMKernel::KokkosScalarLMKernel(const InputParameters & parameters)
  : KernelScalarBase<Kernel>(parameters),
    _kappa(kokkosCoupledScalarValue("kappa")),
    _value(getParam<Real>("value")),
    _penalty(getParam<Real>("penalty")),
    _pp_value(getPostprocessorValue("pp_name"))
{
}
