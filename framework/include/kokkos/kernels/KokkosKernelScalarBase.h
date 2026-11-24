//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosKernel.h"

#include "MooseVariableScalar.h"

namespace Moose::Kokkos
{

/**
 * Mixin that lets a Kokkos field kernel contribute to the rows of a coupled scalar variable, the
 * Kokkos analog of the host KernelScalarBase. The base class must be Kernel or derive from it.
 *
 * The derived class may define the following hooks as inlined public methods (not virtual
 * overrides). Each returns a quadrature-point integrand that is multiplied by JxW and summed over
 * the elements of the kernel blocks:
 *
 * // Scalar residual entry h (0 <= h < scalar variable order)
 * template <typename Derived>
 * KOKKOS_FUNCTION Real computeQpScalarResidual(const unsigned int h,
 *                                              const unsigned int qp,
 *                                              AssemblyDatum & datum) const;
 * // d(scalar residual h) / d(scalar DOF m)
 * template <typename Derived>
 * KOKKOS_FUNCTION Real computeQpScalarJacobian(const unsigned int h,
 *                                              const unsigned int m,
 *                                              const unsigned int qp,
 *                                              AssemblyDatum & datum) const;
 * // d(scalar residual h) / d(field variable jvar, element-local DOF j)
 * template <typename Derived>
 * KOKKOS_FUNCTION Real computeQpScalarOffDiagJacobian(const unsigned int h,
 *                                                     const unsigned int j,
 *                                                     const unsigned int jvar,
 *                                                     const unsigned int qp,
 *                                                     AssemblyDatum & datum) const;
 *
 * The scalar residual and the scalar-scalar Jacobian are reduced over the elements into
 * k or k^2 values (k being the scalar variable order) and then added once into the scalar DOF
 * entries, instead of atomically adding every quadrature point contribution into the same few
 * addresses. On ranks that do not own the scalar DOFs, the contributions go into the ghost
 * entries and are summed into the owner by the ghost communication of the Kokkos system. The
 * scalar-field Jacobian is called for every field variable coupled to the scalar variable rows,
 * and the hook is expected to return zero for the field variables it does not depend on.
 */
template <typename Base>
class KernelScalarBase : public Base
{
public:
  static InputParameters validParams();

  /// This object has the scalar residual and Jacobian hooks
  static constexpr bool supports_scalar_residual = true;

  KernelScalarBase(const InputParameters & parameters);
  /**
   * Copy constructor for parallel dispatch
   */
  KernelScalarBase(const KernelScalarBase & object);

  /**
   * Kokkos function tags
   */
  ///@{
  struct ScalarResidualLoop
  {
  };
  struct ScalarJacobianLoop
  {
  };
  struct ScalarOffDiagJacobianLoop
  {
  };
  struct ScalarAccumulateLoop
  {
  };
  ///@}

  virtual void computeResidual() override;
  virtual void computeJacobian() override;

  /// The kernel contributes to the rows of the scalar variable
  virtual std::set<std::string> additionalROVariables() override;

  /**
   * Dispatch the scalar residual, the scalar-scalar Jacobian, and the scalar-field Jacobian
   * calculations. Derived classes may override these to restrict which kernel instances
   * contribute.
   */
  ///@{
  virtual void computeScalarResidual();
  virtual void computeScalarJacobian();
  virtual void computeScalarOffDiagJacobian();
  ///@}

  /**
   * Default hook methods to prevent compile errors even when these methods were not defined in
   * the derived class
   */
  ///@{
  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarResidual(const unsigned int /* h */,
                                               const unsigned int /* qp */,
                                               AssemblyDatum & /* datum */) const
  {
    ::Kokkos::abort("Default computeQpScalarResidual() should never be called. Make sure you "
                    "properly redefined this method in your class without typos.");

    return 0;
  }
  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarJacobian(const unsigned int /* h */,
                                               const unsigned int /* m */,
                                               const unsigned int /* qp */,
                                               AssemblyDatum & /* datum */) const
  {
    ::Kokkos::abort("Default computeQpScalarJacobian() should never be called. Make sure you "
                    "properly redefined this method in your class without typos.");

    return 0;
  }
  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarOffDiagJacobian(const unsigned int /* h */,
                                                      const unsigned int /* j */,
                                                      const unsigned int /* jvar */,
                                                      const unsigned int /* qp */,
                                                      AssemblyDatum & /* datum */) const
  {
    ::Kokkos::abort("Default computeQpScalarOffDiagJacobian() should never be called. Make sure "
                    "you properly redefined this method in your class without typos.");

    return 0;
  }
  ///@}

  /**
   * Functions used to check if users have overriden the hook methods
   * @returns The function pointer of the default hook method
   */
  ///@{
  template <typename Derived>
  static auto defaultScalarResidual()
  {
    return &KernelScalarBase::computeQpScalarResidual<Derived>;
  }
  template <typename Derived>
  static auto defaultScalarJacobian()
  {
    return &KernelScalarBase::computeQpScalarJacobian<Derived>;
  }
  template <typename Derived>
  static auto defaultScalarOffDiagJacobian()
  {
    return &KernelScalarBase::computeQpScalarOffDiagJacobian<Derived>;
  }
  ///@}

  /**
   * The parallel computation entry functions called by Kokkos
   */
  ///@{
  using Base::operator();
  template <typename Derived>
  KOKKOS_FUNCTION void
  operator()(ScalarResidualLoop, const ThreadID tid, const Derived & kernel, Real * result) const;
  template <typename Derived>
  KOKKOS_FUNCTION void
  operator()(ScalarJacobianLoop, const ThreadID tid, const Derived & kernel, Real * result) const;
  template <typename Derived>
  KOKKOS_FUNCTION void
  operator()(ScalarOffDiagJacobianLoop, const ThreadID tid, const Derived & kernel) const;
  template <typename Derived>
  KOKKOS_FUNCTION void
  operator()(ScalarAccumulateLoop, const ThreadID tid, const Derived & kernel) const;
  ///@}

  /**
   * Compute the scalar-field Jacobian of an element and a coupled field variable. Can be
   * customized in the derived class by defining it with the same signature, for example to
   * precompute element-level quantities shared by all entries.
   * @param kernel The kernel object of the final derived type
   * @param datum The AssemblyDatum object of the current thread, whose jvar is the field variable
   */
  template <typename Derived>
  KOKKOS_FUNCTION void computeScalarOffDiagJacobianInternal(const Derived & kernel,
                                                            AssemblyDatum & datum) const;

  /**
   * Functions required by the Kokkos reducer concept: sum of _num_reduced_values values
   */
  ///@{
  template <typename Derived>
  KOKKOS_FUNCTION void join(Real * result, const Real * source) const
  {
    for (unsigned int i = 0; i < _num_reduced_values; ++i)
      result[i] += source[i];
  }
  template <typename Derived>
  KOKKOS_FUNCTION void init(Real * result) const
  {
    for (unsigned int i = 0; i < _num_reduced_values; ++i)
      result[i] = 0;
  }
  ///@}

protected:
  /**
   * Accumulate a scalar-field Jacobian entry into the tagged matrices
   * @param local_ke The Jacobian entry, before scaling
   * @param datum The AssemblyDatum object of the current thread
   * @param h The scalar DOF index of the row
   * @param j The element-local DOF index of the column in datum.jvar()
   */
  KOKKOS_FUNCTION void accumulateTaggedScalarOffDiagMatrix(const Real local_ke,
                                                           const AssemblyDatum & datum,
                                                           const unsigned int h,
                                                           const unsigned int j) const
  {
    if (!local_ke)
      return;

    auto & sys = this->kokkosSystem(this->_kokkos_var.sys());
    this->accumulateTaggedDofMatrix(local_ke * sys.getVariableScalingFactor(_kappa_var),
                                    sys,
                                    sys.getScalarLocalDofIndex(h, _kappa_var),
                                    sys.getElemLocalDofIndex(datum.elem().id, j, datum.jvar()));
  }

  /// Whether to compute the scalar rows
  const bool _compute_scalar_residuals;
  /// The scalar variable
  const MooseVariableScalar * const _kappa_var_ptr;
  /// The scalar variable number
  const unsigned int _kappa_var;
  /// The order of the scalar variable, i.e. its number of DOFs
  const unsigned int _k_order;

private:
  /**
   * Reduce the integrals over the kernel blocks and add them into the scalar DOF entries
   * @tparam Operation The reducer function tag
   * @param dispatcher The cached reducer of the operation
   * @param num_values The number of reduced values (k or k^2)
   * @param offset The offset of the values in the accumulation buffer (0 for the residual, k for
   * the Jacobian)
   */
  template <typename Operation>
  void reduceAndAccumulate(std::unique_ptr<DispatcherBase> & dispatcher,
                           const unsigned int num_values,
                           const unsigned int offset);

  /// Number of values in the current reduction
  unsigned int _num_reduced_values = 0;
  /// Reduced scalar residual (first k entries) and scalar-scalar Jacobian (next k^2 entries)
  Array<Real> _scalar_values;

  /**
   * Kokkos functor dispatchers
   */
  ///@{
  std::unique_ptr<DispatcherBase> _scalar_residual_dispatcher;
  std::unique_ptr<DispatcherBase> _scalar_jacobian_dispatcher;
  std::unique_ptr<DispatcherBase> _scalar_offdiag_jacobian_dispatcher;
  std::unique_ptr<DispatcherBase> _scalar_accumulate_dispatcher;
  ///@}
};

template <typename Base>
InputParameters
KernelScalarBase<Base>::validParams()
{
  auto params = Base::validParams();
  // This parameter can get renamed in derived class to a more relevant variable name
  params.addRequiredCoupledVar("scalar_variable", "Primary coupled scalar variable");
  params.template addParam<bool>(
      "compute_scalar_residuals", true, "Whether to compute scalar residuals");
  return params;
}

template <typename Base>
KernelScalarBase<Base>::KernelScalarBase(const InputParameters & parameters)
  : Base(parameters),
    _compute_scalar_residuals(this->template getParam<bool>("compute_scalar_residuals")),
    _kappa_var_ptr(this->getScalarVar("scalar_variable", 0)),
    _kappa_var(_kappa_var_ptr->number()),
    _k_order(_kappa_var_ptr->order())
{
  if (this->_vars.size() > 1)
    this->paramError("variables",
                     "Kokkos kernels with scalar variable rows support only a single variable.");
  if (_kappa_var_ptr->sys().number() != this->_vars[0]->sys().number())
    this->paramError("scalar_variable",
                     "The scalar variable must be in the same system as the kernel variable.");

  _scalar_values.create(_k_order + _k_order * _k_order);
}

template <typename Base>
KernelScalarBase<Base>::KernelScalarBase(const KernelScalarBase & object)
  : Base(object),
    _compute_scalar_residuals(object._compute_scalar_residuals),
    _kappa_var_ptr(object._kappa_var_ptr),
    _kappa_var(object._kappa_var),
    _k_order(object._k_order),
    _num_reduced_values(object._num_reduced_values),
    _scalar_values(object._scalar_values)
{
}

template <typename Base>
std::set<std::string>
KernelScalarBase<Base>::additionalROVariables()
{
  auto vars = Base::additionalROVariables();
  vars.insert(_kappa_var_ptr->name());
  return vars;
}

template <typename Base>
void
KernelScalarBase<Base>::computeResidual()
{
  Base::computeResidual();

  if (_compute_scalar_residuals)
    computeScalarResidual();
}

template <typename Base>
void
KernelScalarBase<Base>::computeJacobian()
{
  Base::computeJacobian();

  if (_compute_scalar_residuals)
  {
    computeScalarJacobian();
    computeScalarOffDiagJacobian();
  }
}

template <typename Base>
void
KernelScalarBase<Base>::computeScalarResidual()
{
  if (DispatcherRegistry::hasUserMethod<ScalarResidualLoop>(this->type()))
    reduceAndAccumulate<ScalarResidualLoop>(_scalar_residual_dispatcher, _k_order, 0);
}

template <typename Base>
void
KernelScalarBase<Base>::computeScalarJacobian()
{
  if (DispatcherRegistry::hasUserMethod<ScalarJacobianLoop>(this->type()))
    reduceAndAccumulate<ScalarJacobianLoop>(
        _scalar_jacobian_dispatcher, _k_order * _k_order, _k_order);
}

template <typename Base>
void
KernelScalarBase<Base>::computeScalarOffDiagJacobian()
{
  if (!DispatcherRegistry::hasUserMethod<ScalarOffDiagJacobianLoop>(this->type()))
    return;

  const auto & coupling =
      this->kokkosSystem(this->_kokkos_var.sys()).getScalarFieldCoupling(_kappa_var);
  if (!coupling.size())
    return;

  this->_thread.resize(coupling.size(), this->numKokkosBlockElements());

  Policy policy(0, this->_thread.size());

  if (!_scalar_offdiag_jacobian_dispatcher)
    _scalar_offdiag_jacobian_dispatcher =
        DispatcherRegistry::build<ScalarOffDiagJacobianLoop>(this, this->type());

  _scalar_offdiag_jacobian_dispatcher->parallelFor(policy);
}

template <typename Base>
template <typename Operation>
void
KernelScalarBase<Base>::reduceAndAccumulate(std::unique_ptr<DispatcherBase> & dispatcher,
                                            const unsigned int num_values,
                                            const unsigned int offset)
{
  // A rank without elements in the kernel blocks contributes nothing
  if (!this->numKokkosBlockElements())
    return;

  ::Kokkos::View<Real *, ::Kokkos::HostSpace> result("scalar_values", num_values);

  _num_reduced_values = num_values;

  if (!dispatcher)
    dispatcher = DispatcherRegistry::build<Operation>(this, this->type());

  dispatcher->parallelReduce(Policy(0, this->numKokkosBlockElements()), result);

  for (const auto i : make_range(num_values))
    _scalar_values[offset + i] = result(i);

  _scalar_values.copyToDevice();

  if (!_scalar_accumulate_dispatcher)
    _scalar_accumulate_dispatcher =
        DispatcherRegistry::build<ScalarAccumulateLoop>(this, this->type());

  _scalar_accumulate_dispatcher->parallelFor(Policy(offset, offset + num_values));
}

template <typename Base>
template <typename Derived>
KOKKOS_FUNCTION void
KernelScalarBase<Base>::operator()(ScalarResidualLoop,
                                   const ThreadID tid,
                                   const Derived & kernel,
                                   Real * result) const
{
  auto elem = this->kokkosBlockElementID(tid);

  AssemblyDatum datum(elem,
                      libMesh::invalid_uint,
                      this->kokkosAssembly(),
                      this->kokkosSystems(),
                      this->_kokkos_var,
                      this->_kokkos_var.var(),
                      0);

  for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
    for (unsigned int h = 0; h < _k_order; ++h)
      result[h] += datum.JxW(qp) * kernel.template computeQpScalarResidual<Derived>(h, qp, datum);
}

template <typename Base>
template <typename Derived>
KOKKOS_FUNCTION void
KernelScalarBase<Base>::operator()(ScalarJacobianLoop,
                                   const ThreadID tid,
                                   const Derived & kernel,
                                   Real * result) const
{
  auto elem = this->kokkosBlockElementID(tid);

  AssemblyDatum datum(elem,
                      libMesh::invalid_uint,
                      this->kokkosAssembly(),
                      this->kokkosSystems(),
                      this->_kokkos_var,
                      _kappa_var,
                      0);

  for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
    for (unsigned int h = 0; h < _k_order; ++h)
      for (unsigned int m = 0; m < _k_order; ++m)
        result[h * _k_order + m] +=
            datum.JxW(qp) * kernel.template computeQpScalarJacobian<Derived>(h, m, qp, datum);
}

template <typename Base>
template <typename Derived>
KOKKOS_FUNCTION void
KernelScalarBase<Base>::operator()(ScalarOffDiagJacobianLoop,
                                   const ThreadID tid,
                                   const Derived & kernel) const
{
  auto elem = this->kokkosBlockElementID(this->_thread(tid, 1));

  auto & sys = this->kokkosSystem(this->_kokkos_var.sys());
  auto jvar = sys.getScalarFieldCoupling(_kappa_var)[this->_thread(tid, 0)];

  if (!sys.isVariableActive(jvar, this->kokkosMesh().getElementInfo(elem).subdomain))
    return;

  AssemblyDatum datum(elem,
                      libMesh::invalid_uint,
                      this->kokkosAssembly(),
                      this->kokkosSystems(),
                      this->_kokkos_var,
                      jvar,
                      0);

  kernel.template computeScalarOffDiagJacobianInternal<Derived>(kernel, datum);
}

template <typename Base>
template <typename Derived>
KOKKOS_FUNCTION void
KernelScalarBase<Base>::computeScalarOffDiagJacobianInternal(const Derived & kernel,
                                                             AssemblyDatum & datum) const
{
  for (unsigned int h = 0; h < _k_order; ++h)
    for (unsigned int j = 0; j < datum.n_jdofs(); ++j)
    {
      Real local_ke = 0;
      for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
        local_ke += datum.JxW(qp) * kernel.template computeQpScalarOffDiagJacobian<Derived>(
                                        h, j, datum.jvar(), qp, datum);

      accumulateTaggedScalarOffDiagMatrix(local_ke, datum, h, j);
    }
}

template <typename Base>
template <typename Derived>
KOKKOS_FUNCTION void
KernelScalarBase<Base>::operator()(ScalarAccumulateLoop,
                                   const ThreadID tid,
                                   const Derived & /* kernel */) const
{
  auto & sys = this->kokkosSystem(this->_kokkos_var.sys());
  const auto value = _scalar_values[tid] * sys.getVariableScalingFactor(_kappa_var);

  if (tid < _k_order)
    this->accumulateTaggedDofResidual(value, sys, sys.getScalarLocalDofIndex(tid, _kappa_var));
  else if (value)
  {
    const auto h = (tid - _k_order) / _k_order;
    const auto m = (tid - _k_order) % _k_order;

    this->accumulateTaggedDofMatrix(value,
                                    sys,
                                    sys.getScalarLocalDofIndex(h, _kappa_var),
                                    sys.getScalarLocalDofIndex(m, _kappa_var));
  }
}

} // namespace Moose::Kokkos
