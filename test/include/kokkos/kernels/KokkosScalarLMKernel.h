//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosKernelScalarBase.h"

/**
 * Kokkos version of ScalarLMKernel: enforces the integral of the variable through a Lagrange
 * multiplier scalar variable, int (u - value / volume + penalty * kappa) dV = 0. The optional
 * penalty term only exists to give the scalar-scalar Jacobian block a nonzero entry for testing.
 */
class KokkosScalarLMKernel : public Moose::Kokkos::KernelScalarBase<Moose::Kokkos::Kernel>
{
public:
  static InputParameters validParams();

  KokkosScalarLMKernel(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpResidual(const unsigned int i,
                                         const unsigned int qp,
                                         AssemblyDatum & datum) const
  {
    return _kappa(datum, 0) * _test(datum, i, qp);
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpOffDiagJacobianScalar(const unsigned int i,
                                                      const unsigned int /* j */,
                                                      const unsigned int jvar,
                                                      const unsigned int qp,
                                                      AssemblyDatum & datum) const
  {
    return jvar == _kappa_var ? _test(datum, i, qp) : 0;
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarResidual(const unsigned int /* h */,
                                               const unsigned int qp,
                                               AssemblyDatum & datum) const
  {
    return _u(datum, qp) - _value / _pp_value + _penalty * _kappa(datum, 0);
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarJacobian(const unsigned int /* h */,
                                               const unsigned int /* m */,
                                               const unsigned int /* qp */,
                                               AssemblyDatum & /* datum */) const
  {
    return _penalty;
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarOffDiagJacobian(const unsigned int /* h */,
                                                      const unsigned int j,
                                                      const unsigned int jvar,
                                                      const unsigned int qp,
                                                      AssemblyDatum & datum) const
  {
    return jvar == _kokkos_var.var() ? _phi(datum, j, qp) : 0;
  }

protected:
  const Moose::Kokkos::VariableValue _kappa;
  const Real _value;
  const Real _penalty;
  const Moose::Kokkos::PostprocessorValue _pp_value;
};
