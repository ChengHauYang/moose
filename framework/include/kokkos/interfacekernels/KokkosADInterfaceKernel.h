//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceKernelBase.h"

#include "KokkosInterfaceDatum.h"

namespace Moose::Kokkos
{

/**
 * Base class for Kokkos two-sided finite element interface kernels using automatic differentiation.
 *
 * Derived classes define an inline public computeQpResidual() method returning ADReal. AD
 * derivatives are assembled for both interface rows and all variables represented in the residual.
 */
class ADInterfaceKernel : public InterfaceKernelBase
{
public:
  static InputParameters validParams();

  ADInterfaceKernel(const InputParameters & parameters);
  ADInterfaceKernel(const ADInterfaceKernel & object);

  virtual void computeResidual() override;
  virtual void computeJacobian() override;
  virtual void computeResidualAndJacobian() override;

  template <typename Derived>
  KOKKOS_FUNCTION void operator()(ResidualLoop, ThreadID tid, const Derived & kernel) const;

protected:
  virtual void dispatch();

  const ADVariableTestValue _test;
  const ADVariableTestGradient _grad_test;
  const ADVariablePhiValue _phi;
  const ADVariablePhiGradient _grad_phi;
  const ADVariableValue _u;
  const ADVariableGradient _grad_u;

  const ADVariableTestValue _test_neighbor;
  const ADVariableTestGradient _grad_test_neighbor;
  const ADVariablePhiValue _phi_neighbor;
  const ADVariablePhiGradient _grad_phi_neighbor;
  const ADVariableValue _u_neighbor;
  const ADVariableGradient _grad_u_neighbor;

  bool _computing_residual = false;
  bool _computing_jacobian = false;

private:
  template <typename Derived>
  KOKKOS_FUNCTION void computeResidualInternal(const Derived & kernel,
                                               const InterfaceFaceRecord & face,
                                               AssemblyDatum & elem_datum,
                                               AssemblyDatum & neighbor_datum,
                                               Moose::DGResidualType type) const;

  KOKKOS_FUNCTION void accumulateTaggedADInterfaceMatrix(const DNDerivativeType & local_ke,
                                                         ContiguousElementID row_elem,
                                                         unsigned int i,
                                                         const Variable & row_var,
                                                         unsigned int comp = 0) const;
};

template <typename Derived>
KOKKOS_FUNCTION void
ADInterfaceKernel::operator()(ResidualLoop, const ThreadID tid, const Derived & kernel) const
{
  const auto & face = kokkosInterfaceFace(_thread(tid, 1));
  auto elem_datum = makePrimaryInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_var, _kokkos_var.var());
  auto neighbor_datum = makeNeighborInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_neighbor_var, _kokkos_neighbor_var.var());

  elem_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));
  neighbor_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));
  elem_datum.do_derivatives(_computing_jacobian);
  neighbor_datum.do_derivatives(_computing_jacobian);

  computeResidualInternal(kernel, face, elem_datum, neighbor_datum, Moose::Element);
  computeResidualInternal(kernel, face, elem_datum, neighbor_datum, Moose::Neighbor);
}

template <typename Derived>
KOKKOS_FUNCTION void
ADInterfaceKernel::computeResidualInternal(const Derived & kernel,
                                           const InterfaceFaceRecord & face,
                                           AssemblyDatum & elem_datum,
                                           AssemblyDatum & neighbor_datum,
                                           const Moose::DGResidualType type) const
{
  auto & row_datum = type == Moose::Element ? elem_datum : neighbor_datum;
  const auto row_elem = type == Moose::Element ? face.elem : face.neighbor;
  const auto & row_var = type == Moose::Element ? _kokkos_var : _kokkos_neighbor_var;

  for (unsigned int i = row_datum.local_thread_id(); i < row_datum.n_idofs();
       i += row_datum.num_local_threads())
  {
    ADReal local_re = 0;

    for (unsigned int qp = 0; qp < face.n_qps; ++qp)
    {
      const auto neighbor_qp = kokkosInterfaceNeighborQp(face, qp);
      local_re += elem_datum.JxW(qp) * kernel.template computeQpResidual<Derived>(
                                           type, i, qp, neighbor_qp, elem_datum, neighbor_datum);
    }

    if (_computing_residual)
      accumulateTaggedInterfaceResidual(local_re.value(), row_elem, i, row_var);
    if (_computing_jacobian)
      accumulateTaggedADInterfaceMatrix(local_re.derivatives(), row_elem, i, row_var);
  }
}

KOKKOS_FUNCTION inline void
ADInterfaceKernel::accumulateTaggedADInterfaceMatrix(const DNDerivativeType & local_ke,
                                                     const ContiguousElementID row_elem,
                                                     const unsigned int i,
                                                     const Variable & row_var,
                                                     const unsigned int comp) const
{
  auto & sys = kokkosSystem(row_var.sys(comp));
  const auto ivar = row_var.var(comp);
  const auto row = sys.getElemLocalDofIndex(row_elem, i, ivar);
  const auto scaling_factor = sys.getVariableScalingFactor(ivar);

  for (unsigned int t = 0; t < _matrix_tags.size(); ++t)
  {
    const auto tag = _matrix_tags[t];

    if (sys.isMatrixTagActive(tag) && !sys.hasNodalBCMatrixTag(row, tag))
      for (unsigned int j = 0; j < local_ke.size(); ++j)
      {
        const auto col = local_ke.raw_index(j);
        ::Kokkos::atomic_add(&sys.getMatrixValue(row, col, tag),
                             local_ke.raw_at(j) * scaling_factor);
      }
  }
}

} // namespace Moose::Kokkos

#define registerKokkosADInterfaceKernel(app, classname)                                            \
  registerKokkosADResidualObject(app, classname)
