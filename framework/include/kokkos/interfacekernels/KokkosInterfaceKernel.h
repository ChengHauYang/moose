//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosInterfaceKernelBase.h"

namespace Moose::Kokkos
{

/**
 * Compile-time dispatch contract for Kokkos two-sided finite element interface kernels.
 *
 * Derived classes provide side-aware residual and four-block Jacobian hooks. Execution remains
 * disabled until the dual-datum assembly loops are implemented.
 */
class InterfaceKernel : public InterfaceKernelBase
{
public:
  static InputParameters validParams();

  InterfaceKernel(const InputParameters & parameters);

  virtual void computeResidual() override;
  virtual void computeJacobian() override;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpResidual(const Moose::DGResidualType /* type */,
                                         const unsigned int /* i */,
                                         const unsigned int /* qp */,
                                         AssemblyDatum & /* elem_datum */,
                                         AssemblyDatum & /* neighbor_datum */) const
  {
    ::Kokkos::abort("Derived Kokkos interface kernels must define computeQpResidual().");
    return 0;
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpJacobian(const Moose::DGJacobianType /* type */,
                                         const unsigned int /* i */,
                                         const unsigned int /* j */,
                                         const unsigned int /* qp */,
                                         AssemblyDatum & /* elem_datum */,
                                         AssemblyDatum & /* neighbor_datum */) const
  {
    ::Kokkos::abort("Derived Kokkos interface kernels must define computeQpJacobian().");
    return 0;
  }

  template <typename Derived>
  KOKKOS_FUNCTION void operator()(ResidualLoop,
                                  const ThreadID tid,
                                  const Derived & kernel) const
  {
    kernel.computeFaceResidual(kernel, kokkosInterfaceFace(tid));
  }

  template <typename Derived>
  KOKKOS_FUNCTION void operator()(JacobianLoop,
                                  const ThreadID tid,
                                  const Derived & kernel) const
  {
    kernel.computeFaceJacobian(kernel, kokkosInterfaceFace(tid));
  }

  template <typename Derived>
  KOKKOS_FUNCTION void computeFaceResidual(const Derived & /* kernel */,
                                           const InterfaceFaceRecord & /* face */) const
  {
    ::Kokkos::abort("Kokkos interface residual assembly is not implemented.");
  }

  template <typename Derived>
  KOKKOS_FUNCTION void computeFaceJacobian(const Derived & /* kernel */,
                                           const InterfaceFaceRecord & /* face */) const
  {
    ::Kokkos::abort("Kokkos interface Jacobian assembly is not implemented.");
  }

protected:
  const VariableTestValue _test;
  const VariableTestGradient _grad_test;
  const VariablePhiValue _phi;
  const VariablePhiGradient _grad_phi;
  const VariableValue _u;
  const VariableGradient _grad_u;

  const VariableTestValue _test_neighbor;
  const VariableTestGradient _grad_test_neighbor;
  const VariablePhiValue _phi_neighbor;
  const VariablePhiGradient _grad_phi_neighbor;
  const VariableValue _u_neighbor;
  const VariableGradient _grad_u_neighbor;
};

} // namespace Moose::Kokkos
