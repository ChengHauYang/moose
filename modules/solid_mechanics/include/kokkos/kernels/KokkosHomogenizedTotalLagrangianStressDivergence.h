//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosTotalLagrangianStressDivergence.h"
#include "KokkosKernelScalarBase.h"
#include "HomogenizationInterface.h"

/**
 * Kokkos version of HomogenizedTotalLagrangianStressDivergence: the total-Lagrangian stress
 * divergence with the homogenization constraint rows of the macro gradient scalar variable.
 *
 * For each constraint (i, j) in the constraint map order, the scalar residual is the domain
 * integral of P(i, j) - target (stress constraint), F(i, j) - delta_ij (strain constraint, large
 * kinematics), or sym(F)(i, j) - delta_ij (strain constraint, small kinematics), where F is the
 * stabilized deformation gradient including the macro gradient. As on the host, only the
 * component 0 instance assembles the scalar residual and the scalar-scalar Jacobian, while each
 * instance assembles the scalar-field Jacobian columns of its own displacement component.
 *
 * The macro gradient is added after the F-bar correction, so its derivatives use the PK1 tangent
 * without the F-bar dF_stab/dF_ust factor (`dpk1_bypass_fbar`). Without F-bar this is the regular
 * PK1 tangent.
 *
 * The constraint targets are evaluated on the host at the origin once per residual evaluation, so
 * they must be functions of time only.
 */
class KokkosHomogenizedTotalLagrangianStressDivergence
  : public HomogenizationInterface<
        Moose::Kokkos::KernelScalarBase<KokkosTotalLagrangianStressDivergence>>
{
  using Real3 = Moose::Kokkos::Real3;
  using Real33 = Moose::Kokkos::Real33;
  using ScalarBase = Moose::Kokkos::KernelScalarBase<KokkosTotalLagrangianStressDivergence>;

public:
  static InputParameters validParams();

  KokkosHomogenizedTotalLagrangianStressDivergence(const InputParameters & parameters);

  virtual void computeResidual() override;
  virtual void computeScalarResidual() override;
  virtual void computeScalarJacobian() override;
  virtual void computeScalarOffDiagJacobian() override;

  /// d(displacement residual) / d(macro gradient component jvar_dof)
  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpOffDiagJacobianScalar(const unsigned int i,
                                                      const unsigned int j,
                                                      const unsigned int jvar,
                                                      const unsigned int qp,
                                                      AssemblyDatum & datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarResidual(const unsigned int h,
                                               const unsigned int qp,
                                               AssemblyDatum & datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarJacobian(const unsigned int h,
                                               const unsigned int m,
                                               const unsigned int qp,
                                               AssemblyDatum & datum) const;

  /// The quadrature point local part of d(scalar residual h) / d(displacement DOF j), that is,
  /// without the non-local F-bar contribution
  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpScalarOffDiagJacobian(const unsigned int h,
                                                      const unsigned int j,
                                                      const unsigned int jvar,
                                                      const unsigned int qp,
                                                      AssemblyDatum & datum) const;

  /// Scalar-field Jacobian of an element, adding the non-local F-bar contribution to the local part
  template <typename Derived>
  KOKKOS_FUNCTION void computeScalarOffDiagJacobianInternal(const Derived & kernel,
                                                            AssemblyDatum & datum) const;

private:
  /**
   * Local part of the change of the constrained quantity h for a unit change of the displacement
   * DOF with trial function gradient grad_phi
   */
  KOKKOS_FUNCTION Real localScalarOffDiagJacobian(const unsigned int h,
                                                  const Real3 & grad_phi,
                                                  const unsigned int qp,
                                                  AssemblyDatum & datum) const;

  /// Whether to include the field-scalar and scalar-field Jacobian blocks
  const bool _use_off_diagonal;

  /// Constraint (i, j) indices and types in the constraint map order
  ///@{
  Moose::Kokkos::Array<unsigned int> _cmap_i;
  Moose::Kokkos::Array<unsigned int> _cmap_j;
  Moose::Kokkos::Array<Homogenization::ConstraintType> _cmap_type;
  ///@}
  /// Constraint targets at the current time
  Moose::Kokkos::Array<Real> _targets;

  /// Stabilized deformation gradient including the macro gradient
  Moose::Kokkos::MaterialProperty<Real, 2> _F;
  /// PK1 tangent without the F-bar dF_stab/dF_ust factor
  Moose::Kokkos::MaterialProperty<Real, 4> _dpk1_bypass_fbar;
  /// Displacement gradients for the F-bar chain of the strain constraints
  const Moose::Kokkos::VariableGradient _grad_disp;
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosHomogenizedTotalLagrangianStressDivergence::computeQpOffDiagJacobianScalar(
    const unsigned int i,
    const unsigned int j,
    const unsigned int jvar,
    const unsigned int qp,
    AssemblyDatum & datum) const
{
  if (!_use_off_diagonal || jvar != _kappa_var)
    return 0;

  const auto bypass = _dpk1_bypass_fbar(datum, qp);
  const auto grad_test = _grad_test(datum, i, qp);
  Real result = 0;
  for (unsigned int J = 0; J < 3; ++J)
    result += bypass(_component, J, _cmap_i[j], _cmap_j[j]) * grad_test(J);
  return result;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosHomogenizedTotalLagrangianStressDivergence::computeQpScalarResidual(
    const unsigned int h, const unsigned int qp, AssemblyDatum & datum) const
{
  const auto i = _cmap_i[h];
  const auto j = _cmap_j[h];

  Real value;
  if (_cmap_type[h] == Homogenization::ConstraintType::Stress)
    value = _pk1_stress(datum, qp)(i, j);
  else
  {
    const auto F = _F(datum, qp);
    value = (_large_kinematics ? F(i, j) : 0.5 * (F(i, j) + F(j, i))) - (i == j);
  }

  return value - _targets[h];
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosHomogenizedTotalLagrangianStressDivergence::computeQpScalarJacobian(
    const unsigned int h, const unsigned int m, const unsigned int qp, AssemblyDatum & datum) const
{
  const auto i = _cmap_i[h];
  const auto j = _cmap_j[h];
  const auto k = _cmap_i[m];
  const auto l = _cmap_j[m];

  if (_cmap_type[h] == Homogenization::ConstraintType::Stress)
    return _dpk1_bypass_fbar(datum, qp)(i, j, k, l);

  if (_large_kinematics)
    return i == k && j == l;

  return 0.5 * (i == k && j == l) + 0.5 * (i == l && j == k);
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosHomogenizedTotalLagrangianStressDivergence::computeQpScalarOffDiagJacobian(
    const unsigned int h,
    const unsigned int j,
    const unsigned int jvar,
    const unsigned int qp,
    AssemblyDatum & datum) const
{
  if (jvar != _kokkos_var.var())
    return 0;

  return localScalarOffDiagJacobian(h, _grad_phi(datum, j, qp), qp, datum);
}

KOKKOS_FUNCTION inline Real
KokkosHomogenizedTotalLagrangianStressDivergence::localScalarOffDiagJacobian(
    const unsigned int h,
    const Real3 & grad_phi,
    const unsigned int qp,
    AssemblyDatum & datum) const
{
  const auto m = _cmap_i[h];
  const auto n = _cmap_j[h];

  if (_cmap_type[h] == Homogenization::ConstraintType::Stress)
  {
    // dPK1/d(grad u) already contains the local F-bar chain
    const auto dpk1 = _dpk1_d_grad_u(datum, qp);
    Real result = 0;
    for (unsigned int L = 0; L < 3; ++L)
      result += dpk1(m, n, _component, L) * grad_phi(L);
    return result;
  }

  // Strain constraint: delta F_stab = dF_stab/dF_ust : (e_component x grad_phi)
  auto delta_F = [&](const unsigned int a, const unsigned int b)
  {
    Real value = (a == _component) * grad_phi(b);

    if (_stabilize_strain_device)
    {
      if (_large_kinematics)
      {
        // dF_stab/dF_ust = gamma I - (gamma / 3) F_ust x F_ust^-T on the displacement-only F_ust
        Real33 F_ust;
        F_ust.identity();
        for (unsigned int c = 0; c < _ndisp; ++c)
        {
          const auto grad_u = _grad_disp(datum, qp, c);
          for (unsigned int d = 0; d < 3; ++d)
            F_ust(c, d) += grad_u(d);
        }
        const Real g3 = _gamma_over_3(datum, qp);
        const auto F_ust_invT_row = F_ust.inverse().transpose().row(_component);
        value = 3 * g3 * value - g3 * F_ust(a, b) * F_ust_invT_row.dot_product(grad_phi);
      }
      else
        // dF_stab/dF_ust = I - (1 / 3) I x I
        value -= (a == b) * grad_phi(_component) / 3;
    }

    return value;
  };

  return _large_kinematics ? delta_F(m, n) : 0.5 * (delta_F(m, n) + delta_F(n, m));
}

template <typename Derived>
KOKKOS_FUNCTION void
KokkosHomogenizedTotalLagrangianStressDivergence::computeScalarOffDiagJacobianInternal(
    const Derived & kernel, AssemblyDatum & datum) const
{
  // Each component instance assembles only its own displacement columns, as on the host
  if (datum.jvar() != _kokkos_var.var())
    return;

  if (!_stabilize_strain_device)
  {
    ScalarBase::computeScalarOffDiagJacobianInternal<Derived>(kernel, datum);
    return;
  }

  const unsigned int n_jdofs = datum.n_jdofs();
  if (n_jdofs > Moose::Kokkos::MAX_CACHED_DOF)
    ::Kokkos::abort("KokkosHomogenizedTotalLagrangianStressDivergence stabilize_strain path: "
                    "element has more trial DOFs per variable than MAX_CACHED_DOF.");

  // Non-local F-bar factor of each trial function, c_j = F_avg^-T(component, :) . avg(grad phi_j)
  // (large kinematics) or c_j = avg(grad phi_j)(component) (small kinematics), as in the base
  // kernel's displacement Jacobian
  Real c[Moose::Kokkos::MAX_CACHED_DOF];
  {
    Real3 F_avg_invT_row(0);
    if (_large_kinematics)
    {
      const auto F_avg_invT = _F_avg_invT(datum, 0);
      F_avg_invT_row =
          Real3(F_avg_invT(_component, 0), F_avg_invT(_component, 1), F_avg_invT(_component, 2));
    }
    else
      F_avg_invT_row(_component) = 1;

    Real vol = 0;
    for (unsigned int j = 0; j < n_jdofs; ++j)
      c[j] = 0;
    for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
    {
      vol += datum.JxW(qp);
      for (unsigned int j = 0; j < n_jdofs; ++j)
        c[j] += datum.JxW(qp) * F_avg_invT_row.dot_product(_grad_phi(datum, j, qp));
    }
    for (unsigned int j = 0; j < n_jdofs; ++j)
      c[j] /= vol;
  }

  for (unsigned int h = 0; h < _k_order; ++h)
  {
    const auto m = _cmap_i[h];
    const auto n = _cmap_j[h];
    const bool stress = _cmap_type[h] == Homogenization::ConstraintType::Stress;

    // Integral of the non-local factor multiplying c_j: (gamma / 3) A(m, n) for stress
    // constraints, the (m, n) component of (gamma / 3) F_ust (large) or (1 / 3) I (small) for
    // strain constraints
    Real nonlocal = 0;
    for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
    {
      Real factor;
      if (stress)
        factor = _gamma_over_3(datum, qp) * _A(datum, qp)(m, n);
      else if (_large_kinematics)
      {
        auto F_ust = [&](const unsigned int a, const unsigned int b)
        { return (a == b) + (a < _ndisp ? _grad_disp(datum, qp, a)(b) : 0); };
        factor = _gamma_over_3(datum, qp) * F_ust(m, n);
      }
      else
        factor = (m == n) / 3.0;
      nonlocal += datum.JxW(qp) * factor;
    }

    for (unsigned int j = 0; j < n_jdofs; ++j)
    {
      Real local_ke = nonlocal * c[j];
      for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
        local_ke +=
            datum.JxW(qp) * localScalarOffDiagJacobian(h, _grad_phi(datum, j, qp), qp, datum);

      accumulateTaggedScalarOffDiagMatrix(local_ke, datum, h, j);
    }
  }
}
