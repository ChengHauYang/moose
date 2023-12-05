//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "FunctionCoupledNodalForce.h"
#include "Function.h"

registerMooseObject("MooseApp", FunctionCoupledNodalForce);

InputParameters
FunctionCoupledNodalForce::validParams()
{
  InputParameters params = CoupledForceNodalKernel::validParams();
  params.addRequiredParam<FunctionName>("function", "The function.");
  
  return params;
}

FunctionCoupledNodalForce::FunctionCoupledNodalForce(const InputParameters & parameters)
  : CoupledForceNodalKernel(parameters),
    _func(getFunction("function"))
{
}

Real
FunctionCoupledNodalForce::computeQpResidual()
{
  return CoupledForceNodalKernel::computeQpResidual() * _func.value(_t, *_current_node);
}

Real
FunctionCoupledNodalForce::computeQpJacobian()
{
  return 0;
}

Real
FunctionCoupledNodalForce::computeQpOffDiagJacobian(unsigned int /*jvar*/)
{
  return 0.0;
}
