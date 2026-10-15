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
  _thread.resize(_num_local_residual_threads, numKokkosInterfaceFaces());
  Policy policy(0, _thread.size());

  if (!_residual_dispatcher)
    _residual_dispatcher = DispatcherRegistry::build<ResidualLoop>(this, type());

  _residual_dispatcher->parallelFor(policy);
}

void
InterfaceKernel::computeJacobian()
{
  if (DispatcherRegistry::hasUserMethod<JacobianLoop>(type()))
  {
    _thread.resize(_num_local_jacobian_threads, numKokkosInterfaceFaces());
    Policy policy(0, _thread.size());

    if (!_jacobian_dispatcher)
      _jacobian_dispatcher = DispatcherRegistry::build<JacobianLoop>(this, type());

    _jacobian_dispatcher->parallelFor(policy);
  }

  if (DispatcherRegistry::hasUserMethod<OffDiagJacobianLoop>(type()))
  {
    const auto & elem_coupling =
        kokkosSystem(_kokkos_var.sys()).getFieldCoupling(_kokkos_var.var());
    const auto & neighbor_coupling =
        kokkosSystem(_kokkos_neighbor_var.sys()).getFieldCoupling(_kokkos_neighbor_var.var());
    const auto n_couplings = std::max(elem_coupling.size(), neighbor_coupling.size());

    _thread.resize(_num_local_jacobian_threads, 2, n_couplings, numKokkosInterfaceFaces());
    Policy policy(0, _thread.size());

    if (!_offdiag_jacobian_dispatcher)
      _offdiag_jacobian_dispatcher = DispatcherRegistry::build<OffDiagJacobianLoop>(this, type());

    _offdiag_jacobian_dispatcher->parallelFor(policy);
  }
}

} // namespace Moose::Kokkos
