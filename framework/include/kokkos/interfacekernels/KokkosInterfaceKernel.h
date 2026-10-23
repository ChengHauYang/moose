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

/** Compile-time base for Kokkos two-sided finite element interface kernels. */
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
                                         const unsigned int /* neighbor_qp */,
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
                                         const unsigned int /* neighbor_qp */,
                                         AssemblyDatum & /* elem_datum */,
                                         AssemblyDatum & /* neighbor_datum */) const
  {
    ::Kokkos::abort("Derived Kokkos interface kernels must define computeQpJacobian().");
    return 0;
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpOffDiagJacobian(const Moose::DGJacobianType /* type */,
                                                const unsigned int /* i */,
                                                const unsigned int /* j */,
                                                const unsigned int /* jvar */,
                                                const unsigned int /* qp */,
                                                const unsigned int /* neighbor_qp */,
                                                AssemblyDatum & /* elem_datum */,
                                                AssemblyDatum & /* neighbor_datum */) const
  {
    ::Kokkos::abort("Derived Kokkos interface kernels must define computeQpOffDiagJacobian().");
    return 0;
  }

  /**
   * Optional per-trial-column preparation hook. The default preserves the existing QP-only
   * interface assembly contract. A derived kernel can return element-local trial-gradient data
   * which is then passed to every face-QP Jacobian evaluation for the column.
   */
  template <typename Derived>
  KOKKOS_FUNCTION Real3 prepareJacobianColumn(const Moose::DGJacobianType,
                                              const unsigned int,
                                              AssemblyDatum &) const
  {
    return Real3(0);
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpJacobianWithColumn(const Moose::DGJacobianType type,
                                                   const unsigned int i,
                                                   const unsigned int j,
                                                   const unsigned int qp,
                                                   const unsigned int neighbor_qp,
                                                   AssemblyDatum & elem_datum,
                                                   AssemblyDatum & neighbor_datum,
                                                   const Real3 &) const
  {
    return static_cast<const Derived &>(*this).template computeQpJacobian<Derived>(
        type, i, j, qp, neighbor_qp, elem_datum, neighbor_datum);
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpOffDiagJacobianWithColumn(const Moose::DGJacobianType type,
                                                          const unsigned int i,
                                                          const unsigned int j,
                                                          const unsigned int jvar,
                                                          const unsigned int qp,
                                                          const unsigned int neighbor_qp,
                                                          AssemblyDatum & elem_datum,
                                                          AssemblyDatum & neighbor_datum,
                                                          const Real3 &) const
  {
    return static_cast<const Derived &>(*this).template computeQpOffDiagJacobian<Derived>(
        type, i, j, jvar, qp, neighbor_qp, elem_datum, neighbor_datum);
  }

  template <typename Derived>
  static auto defaultJacobian()
  {
    return &InterfaceKernel::computeQpJacobian<Derived>;
  }

  template <typename Derived>
  static auto defaultOffDiagJacobian()
  {
    return &InterfaceKernel::computeQpOffDiagJacobian<Derived>;
  }

  template <typename Derived>
  KOKKOS_FUNCTION void operator()(ResidualLoop, const ThreadID tid, const Derived & kernel) const;

  template <typename Derived>
  KOKKOS_FUNCTION void operator()(JacobianLoop, const ThreadID tid, const Derived & kernel) const;

  template <typename Derived>
  KOKKOS_FUNCTION void
  operator()(OffDiagJacobianLoop, const ThreadID tid, const Derived & kernel) const;

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

private:
  template <typename Derived>
  KOKKOS_FUNCTION void computeResidualInternal(const Derived & kernel,
                                               const InterfaceFaceRecord & face,
                                               AssemblyDatum & elem_datum,
                                               AssemblyDatum & neighbor_datum,
                                               Moose::DGResidualType type) const;

  template <typename Derived>
  KOKKOS_FUNCTION void computeJacobianInternal(const Derived & kernel,
                                               const InterfaceFaceRecord & face,
                                               AssemblyDatum & elem_datum,
                                               AssemblyDatum & neighbor_datum,
                                               Moose::DGJacobianType type) const;

  template <typename Derived>
  KOKKOS_FUNCTION void computeOffDiagJacobianInternal(const Derived & kernel,
                                                      const InterfaceFaceRecord & face,
                                                      AssemblyDatum & elem_datum,
                                                      AssemblyDatum & neighbor_datum,
                                                      Moose::DGJacobianType type) const;
};

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceKernel::operator()(ResidualLoop, const ThreadID tid, const Derived & kernel) const
{
  const auto & face = kokkosInterfaceFace(_thread(tid, 1));
  auto elem_datum = makePrimaryInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_var, _kokkos_var.var());
  auto neighbor_datum = makeNeighborInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_neighbor_var, _kokkos_neighbor_var.var());

  elem_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));
  neighbor_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));

  computeResidualInternal(kernel, face, elem_datum, neighbor_datum, Moose::Element);
  computeResidualInternal(kernel, face, elem_datum, neighbor_datum, Moose::Neighbor);
}

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceKernel::operator()(JacobianLoop, const ThreadID tid, const Derived & kernel) const
{
  const auto & face = kokkosInterfaceFace(_thread(tid, 1));

  auto elem_column_datum = makePrimaryInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_var, _kokkos_var.var());
  auto neighbor_row_elem_column_datum = makeNeighborInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_neighbor_var, _kokkos_var.var());
  auto elem_row_neighbor_column_datum = makePrimaryInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_var, _kokkos_neighbor_var.var());
  auto neighbor_column_datum = makeNeighborInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), _kokkos_neighbor_var, _kokkos_neighbor_var.var());

  elem_column_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));
  neighbor_row_elem_column_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));
  elem_row_neighbor_column_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));
  neighbor_column_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));

  computeJacobianInternal(
      kernel, face, elem_column_datum, neighbor_row_elem_column_datum, Moose::ElementElement);
  computeJacobianInternal(
      kernel, face, elem_row_neighbor_column_datum, neighbor_column_datum, Moose::ElementNeighbor);
  computeJacobianInternal(
      kernel, face, elem_column_datum, neighbor_row_elem_column_datum, Moose::NeighborElement);
  computeJacobianInternal(
      kernel, face, elem_row_neighbor_column_datum, neighbor_column_datum, Moose::NeighborNeighbor);
}

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceKernel::operator()(OffDiagJacobianLoop, const ThreadID tid, const Derived & kernel) const
{
  const bool row_is_neighbor = _thread(tid, 1);
  const auto & row_var = row_is_neighbor ? _kokkos_neighbor_var : _kokkos_var;
  const auto & coupling = kokkosSystem(row_var.sys()).getFieldCoupling(row_var.var());
  const auto coupling_index = _thread(tid, 2);
  if (coupling_index >= coupling.size())
    return;

  const auto jvar = coupling[coupling_index];
  if (jvar == _kokkos_var.var() || jvar == _kokkos_neighbor_var.var())
    return;

  const auto & face = kokkosInterfaceFace(_thread(tid, 3));
  const auto & system = kokkosSystem(row_var.sys());
  const bool active_on_elem =
      system.isVariableActive(jvar, kokkosMesh().getElementInfo(face.elem).subdomain);
  const bool active_on_neighbor =
      system.isVariableActive(jvar, kokkosMesh().getElementInfo(face.neighbor).subdomain);

  if (!active_on_elem && !active_on_neighbor)
    return;

  auto elem_datum = makePrimaryInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), row_var, jvar);
  auto neighbor_datum = makeNeighborInterfaceDatum<AssemblyDatum>(
      face, kokkosAssembly(), kokkosSystems(), row_var, jvar);

  elem_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));
  neighbor_datum.set_local_parallel(_thread(tid, 0), _thread.size(0));

  if (!row_is_neighbor)
  {
    if (active_on_elem)
      computeOffDiagJacobianInternal(
          kernel, face, elem_datum, neighbor_datum, Moose::ElementElement);
    if (active_on_neighbor)
      computeOffDiagJacobianInternal(
          kernel, face, elem_datum, neighbor_datum, Moose::ElementNeighbor);
  }
  else
  {
    if (active_on_elem)
      computeOffDiagJacobianInternal(
          kernel, face, elem_datum, neighbor_datum, Moose::NeighborElement);
    if (active_on_neighbor)
      computeOffDiagJacobianInternal(
          kernel, face, elem_datum, neighbor_datum, Moose::NeighborNeighbor);
  }
}

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceKernel::computeResidualInternal(const Derived & kernel,
                                         const InterfaceFaceRecord & face,
                                         AssemblyDatum & elem_datum,
                                         AssemblyDatum & neighbor_datum,
                                         const Moose::DGResidualType type) const
{
  auto & row_datum = type == Moose::Element ? elem_datum : neighbor_datum;
  const auto row_elem = type == Moose::Element ? face.elem : face.neighbor;
  const auto & row_var = type == Moose::Element ? _kokkos_var : _kokkos_neighbor_var;
  Real local_re[MAX_CACHED_DOF];

  const unsigned int stride = MAX_CACHED_DOF * row_datum.num_local_threads();
  unsigned int num_batches = row_datum.n_idofs() / stride;
  if (row_datum.n_idofs() % stride)
    ++num_batches;

  for (unsigned int batch = 0; batch < num_batches; ++batch)
  {
    unsigned int ib = batch * stride;
    unsigned int ie = ::Kokkos::min(ib + stride, row_datum.n_idofs());
    const unsigned int n = ie - ib;
    const unsigned int d = n / row_datum.num_local_threads();
    const unsigned int m = n % row_datum.num_local_threads();
    const unsigned int t = row_datum.local_thread_id();
    ib += t * d + (t < m ? t : m);
    ie = ib + d + (t < m ? 1 : 0);

    for (unsigned int i = ib; i < ie; ++i)
      local_re[i - ib] = 0;

    for (unsigned int qp = 0; qp < face.n_qps; ++qp)
    {
      const auto neighbor_qp = kokkosInterfaceNeighborQp(face, qp);
      for (unsigned int i = ib; i < ie; ++i)
        local_re[i - ib] +=
            elem_datum.JxW(qp) * kernel.template computeQpResidual<Derived>(
                                     type, i, qp, neighbor_qp, elem_datum, neighbor_datum);
    }

    for (unsigned int i = ib; i < ie; ++i)
      accumulateTaggedInterfaceResidual(local_re[i - ib], row_elem, i, row_var);
  }
}

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceKernel::computeJacobianInternal(const Derived & kernel,
                                         const InterfaceFaceRecord & face,
                                         AssemblyDatum & elem_datum,
                                         AssemblyDatum & neighbor_datum,
                                         const Moose::DGJacobianType type) const
{
  const bool row_is_neighbor = type == Moose::NeighborElement || type == Moose::NeighborNeighbor;
  const bool column_is_neighbor = type == Moose::ElementNeighbor || type == Moose::NeighborNeighbor;
  auto & row_datum = row_is_neighbor ? neighbor_datum : elem_datum;
  auto & column_datum = column_is_neighbor ? neighbor_datum : elem_datum;
  const auto row_elem = row_is_neighbor ? face.neighbor : face.elem;
  const auto col_elem = column_is_neighbor ? face.neighbor : face.elem;
  const auto & row_var = row_is_neighbor ? _kokkos_neighbor_var : _kokkos_var;
  const auto & col_var = column_is_neighbor ? _kokkos_neighbor_var : _kokkos_var;
  Real local_ke[MAX_CACHED_DOF];

  for (unsigned int j = row_datum.local_thread_id(); j < column_datum.n_jdofs();
       j += row_datum.num_local_threads())
  {
    const auto counterpart = column_is_neighbor ? face.elem : face.neighbor;
    // The column element can be a one-layer ghost on distributed meshes. Its volume geometry is
    // not guaranteed to be present in the assembly cache, so construct it directly from the mesh.
    AssemblyDatum volume_column_datum(
        column_datum, col_elem, libMesh::invalid_uint, counterpart, true);
    const auto column_data =
        kernel.template prepareJacobianColumn<Derived>(type, j, volume_column_datum);

    unsigned int num_batches = row_datum.n_idofs() / MAX_CACHED_DOF;
    if (row_datum.n_idofs() % MAX_CACHED_DOF)
      ++num_batches;

    for (unsigned int batch = 0; batch < num_batches; ++batch)
    {
      const unsigned int ib = batch * MAX_CACHED_DOF;
      const unsigned int ie = ::Kokkos::min(ib + MAX_CACHED_DOF, row_datum.n_idofs());
      for (unsigned int i = ib; i < ie; ++i)
        local_ke[i - ib] = 0;

      for (unsigned int qp = 0; qp < face.n_qps; ++qp)
      {
        const auto neighbor_qp = kokkosInterfaceNeighborQp(face, qp);
        for (unsigned int i = ib; i < ie; ++i)
          local_ke[i - ib] +=
              elem_datum.JxW(qp) *
              kernel.template computeQpJacobianWithColumn<Derived>(
                  type, i, j, qp, neighbor_qp, elem_datum, neighbor_datum, column_data);
      }

      for (unsigned int i = ib; i < ie; ++i)
        accumulateTaggedInterfaceMatrix(
            local_ke[i - ib], row_elem, col_elem, i, j, row_var, col_var.var());
    }
  }
}

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceKernel::computeOffDiagJacobianInternal(const Derived & kernel,
                                                const InterfaceFaceRecord & face,
                                                AssemblyDatum & elem_datum,
                                                AssemblyDatum & neighbor_datum,
                                                const Moose::DGJacobianType type) const
{
  const bool row_is_neighbor = type == Moose::NeighborElement || type == Moose::NeighborNeighbor;
  const bool column_is_neighbor = type == Moose::ElementNeighbor || type == Moose::NeighborNeighbor;
  auto & row_datum = row_is_neighbor ? neighbor_datum : elem_datum;
  auto & column_datum = column_is_neighbor ? neighbor_datum : elem_datum;
  const auto row_elem = row_is_neighbor ? face.neighbor : face.elem;
  const auto col_elem = column_is_neighbor ? face.neighbor : face.elem;
  const auto & row_var = row_is_neighbor ? _kokkos_neighbor_var : _kokkos_var;
  Real local_ke[MAX_CACHED_DOF];

  for (unsigned int j = row_datum.local_thread_id(); j < column_datum.n_jdofs();
       j += row_datum.num_local_threads())
  {
    const auto counterpart = column_is_neighbor ? face.elem : face.neighbor;
    // The column element can be a one-layer ghost on distributed meshes. Its volume geometry is
    // not guaranteed to be present in the assembly cache, so construct it directly from the mesh.
    AssemblyDatum volume_column_datum(
        column_datum, col_elem, libMesh::invalid_uint, counterpart, true);
    const auto column_data =
        kernel.template prepareJacobianColumn<Derived>(type, j, volume_column_datum);

    unsigned int num_batches = row_datum.n_idofs() / MAX_CACHED_DOF;
    if (row_datum.n_idofs() % MAX_CACHED_DOF)
      ++num_batches;

    for (unsigned int batch = 0; batch < num_batches; ++batch)
    {
      const unsigned int ib = batch * MAX_CACHED_DOF;
      const unsigned int ie = ::Kokkos::min(ib + MAX_CACHED_DOF, row_datum.n_idofs());
      for (unsigned int i = ib; i < ie; ++i)
        local_ke[i - ib] = 0;

      for (unsigned int qp = 0; qp < face.n_qps; ++qp)
      {
        const auto neighbor_qp = kokkosInterfaceNeighborQp(face, qp);
        for (unsigned int i = ib; i < ie; ++i)
          local_ke[i - ib] +=
              elem_datum.JxW(qp) *
              kernel.template computeQpOffDiagJacobianWithColumn<Derived>(type,
                                                                          i,
                                                                          j,
                                                                          row_datum.jvar(),
                                                                          qp,
                                                                          neighbor_qp,
                                                                          elem_datum,
                                                                          neighbor_datum,
                                                                          column_data);
      }

      for (unsigned int i = ib; i < ie; ++i)
        accumulateTaggedInterfaceMatrix(
            local_ke[i - ib], row_elem, col_elem, i, j, row_var, row_datum.jvar());
    }
  }
}

} // namespace Moose::Kokkos

#define registerKokkosInterfaceKernel(app, classname) registerKokkosResidualObject(app, classname)
