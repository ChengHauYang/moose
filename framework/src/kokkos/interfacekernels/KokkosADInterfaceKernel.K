//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosADInterfaceKernel.h"

namespace Moose::Kokkos
{

InputParameters
ADInterfaceKernel::validParams()
{
  return InterfaceKernelBase::validParams();
}

ADInterfaceKernel::ADInterfaceKernel(const InputParameters & parameters)
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

ADInterfaceKernel::ADInterfaceKernel(const ADInterfaceKernel & object)
  : InterfaceKernelBase(object),
    _test(object._test),
    _grad_test(object._grad_test),
    _phi(object._phi),
    _grad_phi(object._grad_phi),
    _u(object._u),
    _grad_u(object._grad_u),
    _test_neighbor(object._test_neighbor),
    _grad_test_neighbor(object._grad_test_neighbor),
    _phi_neighbor(object._phi_neighbor),
    _grad_phi_neighbor(object._grad_phi_neighbor),
    _u_neighbor(object._u_neighbor),
    _grad_u_neighbor(object._grad_u_neighbor),
    _computing_residual(object._computing_residual),
    _computing_jacobian(object._computing_jacobian)
{
}

void
ADInterfaceKernel::computeResidual()
{
  _computing_residual = true;
  _computing_jacobian = false;
  dispatch();
}

void
ADInterfaceKernel::computeJacobian()
{
  _computing_residual = false;
  _computing_jacobian = true;
  dispatch();
}

void
ADInterfaceKernel::computeResidualAndJacobian()
{
  _computing_residual = true;
  _computing_jacobian = true;
  dispatch();
}

void
ADInterfaceKernel::dispatch()
{
  _thread.resize(_computing_jacobian ? _num_local_jacobian_threads : _num_local_residual_threads,
                 numKokkosInterfaceFaces());
  Policy policy(0, _thread.size());

  if (!_residual_dispatcher)
    _residual_dispatcher = DispatcherRegistry::build<ResidualLoop>(this, type());

  _residual_dispatcher->parallelFor(policy);
}

} // namespace Moose::Kokkos
