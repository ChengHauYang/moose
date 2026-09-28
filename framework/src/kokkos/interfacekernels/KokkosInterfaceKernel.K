//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosInterfaceKernel.h"

namespace Moose::Kokkos
{

InputParameters
InterfaceKernel::validParams()
{
  return InterfaceKernelBase::validParams();
}

InterfaceKernel::InterfaceKernel(const InputParameters & parameters)
  : InterfaceKernelBase(parameters),
    _test(),
    _grad_test(),
    _phi(),
    _grad_phi(),
    _u(_var),
    _grad_u(_var),
    _test_neighbor(),
    _grad_test_neighbor(),
    _phi_neighbor(),
    _grad_phi_neighbor(),
    _u_neighbor(_neighbor_var),
    _grad_u_neighbor(_neighbor_var)
{
}

void
InterfaceKernel::computeResidual()
{
  mooseError("Kokkos interface residual assembly is not implemented.");
}

void
InterfaceKernel::computeJacobian()
{
  mooseError("Kokkos interface Jacobian assembly is not implemented.");
}

} // namespace Moose::Kokkos
