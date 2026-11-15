//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosShiftedInterfaceKernel.h"

/**
 * Kokkos shifted cohesive-zone interface kernel for total-Lagrangian finite-strain displacements.
 */
class KokkosSCZMInterfaceKernelTotalLagrangian : public Moose::Kokkos::ShiftedInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosSCZMInterfaceKernelTotalLagrangian(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpResidual(Moose::DGResidualType type,
                                         unsigned int i,
                                         unsigned int qp,
                                         unsigned int neighbor_qp,
                                         AssemblyDatum & elem_datum,
                                         AssemblyDatum & neighbor_datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpJacobian(Moose::DGJacobianType type,
                                         unsigned int i,
                                         unsigned int j,
                                         unsigned int qp,
                                         unsigned int neighbor_qp,
                                         AssemblyDatum & elem_datum,
                                         AssemblyDatum & neighbor_datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpOffDiagJacobian(Moose::DGJacobianType type,
                                                unsigned int i,
                                                unsigned int j,
                                                unsigned int jvar,
                                                unsigned int qp,
                                                unsigned int neighbor_qp,
                                                AssemblyDatum & elem_datum,
                                                AssemblyDatum & neighbor_datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION Moose::Kokkos::Real3 prepareJacobianColumn(
      Moose::DGJacobianType type, unsigned int j, AssemblyDatum & volume_column_datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpJacobianWithColumn(Moose::DGJacobianType type,
                                                   unsigned int i,
                                                   unsigned int j,
                                                   unsigned int qp,
                                                   unsigned int neighbor_qp,
                                                   AssemblyDatum & elem_datum,
                                                   AssemblyDatum & neighbor_datum,
                                                   const Moose::Kokkos::Real3 & avg_grad_phi) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real
  computeQpOffDiagJacobianWithColumn(Moose::DGJacobianType type,
                                     unsigned int i,
                                     unsigned int j,
                                     unsigned int jvar,
                                     unsigned int qp,
                                     unsigned int neighbor_qp,
                                     AssemblyDatum & elem_datum,
                                     AssemblyDatum & neighbor_datum,
                                     const Moose::Kokkos::Real3 & avg_grad_phi) const;

protected:
  KOKKOS_FUNCTION Real computeDResidualDDisplacement(Moose::DGJacobianType type,
                                                     unsigned int i,
                                                     unsigned int j,
                                                     unsigned int component_j,
                                                     unsigned int qp,
                                                     unsigned int neighbor_qp,
                                                     AssemblyDatum & elem_datum,
                                                     AssemblyDatum & neighbor_datum) const;

  KOKKOS_FUNCTION Real computeDResidualDDeformationGradient(Moose::DGJacobianType type,
                                                            unsigned int i,
                                                            unsigned int j,
                                                            unsigned int component_j,
                                                            unsigned int qp,
                                                            unsigned int neighbor_qp,
                                                            AssemblyDatum & elem_datum,
                                                            AssemblyDatum & neighbor_datum) const;

  KOKKOS_FUNCTION Real
  computeDirectionalCorrectionJacobian(Moose::DGJacobianType type,
                                       unsigned int i,
                                       unsigned int j,
                                       unsigned int component_j,
                                       unsigned int qp,
                                       unsigned int neighbor_qp,
                                       AssemblyDatum & elem_datum,
                                       AssemblyDatum & neighbor_datum,
                                       const Moose::Kokkos::Real3 & avg_grad_phi) const;

  /**
   * Contribution of the F-bar deformation gradient B to the row-side directional correction
   * derivative n_t . dP / du_j, where P = P(U, B) and B = (det(M) / det(U))^(1/3) U. This adds the
   * local terms gamma C_B - gamma / 3 Q (x) U^{-T} and the element-volume term
   * gamma / 3 Q (x) M^{-T} contracted with the volume-averaged trial gradient avg_grad_phi.
   */
  KOKKOS_FUNCTION Real
  computeFbarDirectionalCorrection(bool on_element,
                                   unsigned int component_j,
                                   unsigned int row_qp,
                                   AssemblyDatum & row_datum,
                                   const Moose::Kokkos::Real3 & normal_tangent,
                                   const Moose::Kokkos::Real3 & grad_phi,
                                   const Moose::Kokkos::Real3 & avg_grad_phi) const;

  KOKKOS_FUNCTION Moose::Kokkos::Real3 normalTangent(AssemblyDatum & datum, unsigned int qp) const
  {
    const auto true_normal = trueNormal(datum, qp);
    const auto surrogate_normal = datum.normals(qp);
    return surrogate_normal - true_normal.dot_product(surrogate_normal) * true_normal;
  }

  const unsigned int _component;
  const unsigned int _ndisp;
  const bool _shifted;
  const bool _directional_correction;
  const bool _volumetric_locking_correction;
  Moose::Kokkos::Array<unsigned int> _disp_vars;
  const Moose::Kokkos::MaterialProperty<Real, 1> _traction_global;
  const Moose::Kokkos::MaterialProperty<Real, 2> _dtraction_djump_global;
  const Moose::Kokkos::MaterialProperty<Real, 3> _dPK1traction_dF;
  Moose::Kokkos::MaterialProperty<Real, 2> _stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _tangent;
  Moose::Kokkos::MaterialProperty<Real, 2> _stress_neighbor;
  Moose::Kokkos::MaterialProperty<Real, 4> _tangent_neighbor;

  /// F-bar data on the element and neighbor sides, used with volumetric locking correction:
  /// d(P)/d(B) at fixed U, the physical deformation gradient U, and its element-volume average M
  ///@{
  Moose::Kokkos::MaterialProperty<Real, 4> _stabilized_tangent;
  Moose::Kokkos::MaterialProperty<Real, 4> _stabilized_tangent_neighbor;
  Moose::Kokkos::MaterialProperty<Real, 2> _physical_deformation_gradient;
  Moose::Kokkos::MaterialProperty<Real, 2> _physical_deformation_gradient_neighbor;
  Moose::Kokkos::MaterialProperty<Real, 2> _average_deformation_gradient;
  Moose::Kokkos::MaterialProperty<Real, 2> _average_deformation_gradient_neighbor;
  ///@}
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeQpResidual(const Moose::DGResidualType type,
                                                            const unsigned int i,
                                                            const unsigned int qp,
                                                            const unsigned int neighbor_qp,
                                                            AssemblyDatum & elem_datum,
                                                            AssemblyDatum & neighbor_datum) const
{
  const auto test = type == Moose::Element ? _test(elem_datum, i, qp)
                                           : _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto sign = type == Moose::Element ? -1.0 : 1.0;

  if (!_shifted)
    return sign * test * _traction_global(elem_datum, qp)(_component);

  const auto residual =
      sign * test * areaCorrection(elem_datum, qp) * _traction_global(elem_datum, qp)(_component);

  if (!_directional_correction)
    return residual;

  const auto normal_tangent = normalTangent(elem_datum, qp);
  const auto stress = type == Moose::Element ? _stress(elem_datum, qp)
                                             : _stress_neighbor(neighbor_datum, neighbor_qp);

  Real directional = 0.0;
  for (unsigned int j = 0; j < _ndisp; ++j)
    directional += stress(_component, j) * normal_tangent(j);

  return type == Moose::Element ? residual - test * directional : residual + test * directional;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeQpJacobian(const Moose::DGJacobianType type,
                                                            const unsigned int i,
                                                            const unsigned int j,
                                                            const unsigned int qp,
                                                            const unsigned int neighbor_qp,
                                                            AssemblyDatum & elem_datum,
                                                            AssemblyDatum & neighbor_datum) const
{
  auto jacobian = computeDResidualDDisplacement(
                      type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum) +
                  computeDResidualDDeformationGradient(
                      type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum);

  if (_shifted && _directional_correction)
    jacobian += computeDirectionalCorrectionJacobian(type,
                                                     i,
                                                     j,
                                                     _component,
                                                     qp,
                                                     neighbor_qp,
                                                     elem_datum,
                                                     neighbor_datum,
                                                     Moose::Kokkos::Real3(0));

  return jacobian;
}

template <typename Derived>
KOKKOS_FUNCTION Moose::Kokkos::Real3
KokkosSCZMInterfaceKernelTotalLagrangian::prepareJacobianColumn(
    const Moose::DGJacobianType type,
    const unsigned int j,
    AssemblyDatum & volume_column_datum) const
{
  if (!_shifted || !_directional_correction || !_volumetric_locking_correction ||
      type == Moose::ElementNeighbor || type == Moose::NeighborElement)
    return Moose::Kokkos::Real3(0);

  bool is_displacement = false;
  for (unsigned int component = 0; component < _ndisp; ++component)
    is_displacement = is_displacement || _disp_vars[component] == volume_column_datum.jvar();
  if (!is_displacement)
    return Moose::Kokkos::Real3(0);

  Moose::Kokkos::Real3 avg_grad_phi(0);
  Real volume = 0.0;
  for (unsigned int volume_qp = 0; volume_qp < volume_column_datum.n_qps(); ++volume_qp)
  {
    const Real weight = volume_column_datum.JxW(volume_qp);
    volume += weight;
    avg_grad_phi += weight * (type == Moose::NeighborNeighbor
                                  ? _grad_phi_neighbor(volume_column_datum, j, volume_qp)
                                  : _grad_phi(volume_column_datum, j, volume_qp));
  }

  const Real inverse_volume = 1.0 / volume;
  for (unsigned int component = 0; component < 3; ++component)
    avg_grad_phi(component) *= inverse_volume;
  return avg_grad_phi;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeQpJacobianWithColumn(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const Moose::Kokkos::Real3 & avg_grad_phi) const
{
  auto jacobian = computeDResidualDDisplacement(
                      type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum) +
                  computeDResidualDDeformationGradient(
                      type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum);

  if (_shifted && _directional_correction)
    jacobian += computeDirectionalCorrectionJacobian(
        type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum, avg_grad_phi);

  return jacobian;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeQpOffDiagJacobian(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int jvar,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum) const
{
  for (unsigned int component_j = 0; component_j < _ndisp; ++component_j)
    if (_disp_vars[component_j] == jvar)
    {
      auto jacobian = computeDResidualDDisplacement(
                          type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum) +
                      computeDResidualDDeformationGradient(
                          type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum);

      if (_shifted && _directional_correction)
        jacobian += computeDirectionalCorrectionJacobian(type,
                                                         i,
                                                         j,
                                                         component_j,
                                                         qp,
                                                         neighbor_qp,
                                                         elem_datum,
                                                         neighbor_datum,
                                                         Moose::Kokkos::Real3(0));

      return jacobian;
    }

  return 0;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeQpOffDiagJacobianWithColumn(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int jvar,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const Moose::Kokkos::Real3 & avg_grad_phi) const
{
  for (unsigned int component_j = 0; component_j < _ndisp; ++component_j)
    if (_disp_vars[component_j] == jvar)
    {
      auto jacobian = computeDResidualDDisplacement(
                          type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum) +
                      computeDResidualDDeformationGradient(
                          type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum);

      if (_shifted && _directional_correction)
        jacobian += computeDirectionalCorrectionJacobian(
            type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum, avg_grad_phi);

      return jacobian;
    }

  return 0;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeDResidualDDisplacement(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum) const
{
  const auto tangent = _dtraction_djump_global(elem_datum, qp)(_component, component_j);

  if (!_shifted)
  {
    switch (type)
    {
      case Moose::ElementElement:
        return _test(elem_datum, i, qp) * tangent * _phi(elem_datum, j, qp);
      case Moose::ElementNeighbor:
        return -_test(elem_datum, i, qp) * tangent * _phi_neighbor(neighbor_datum, j, neighbor_qp);
      case Moose::NeighborElement:
        return -_test_neighbor(neighbor_datum, i, neighbor_qp) * tangent * _phi(elem_datum, j, qp);
      case Moose::NeighborNeighbor:
        return _test_neighbor(neighbor_datum, i, neighbor_qp) * tangent *
               _phi_neighbor(neighbor_datum, j, neighbor_qp);
    }
    return 0;
  }

  const auto distance = surrogateDistanceVector(elem_datum, qp);
  const auto coefficient = areaCorrection(elem_datum, qp) * tangent;

  Real res = 0.0;
  switch (type)
  {
    case Moose::ElementElement:
      res = _test(elem_datum, i, qp) * coefficient *
            shiftedPhi(_phi, _grad_phi, elem_datum, distance, j, qp);
      break;
    case Moose::ElementNeighbor:
      res = -_test(elem_datum, i, qp) * coefficient *
            shiftedPhi(_phi_neighbor, _grad_phi_neighbor, neighbor_datum, distance, j, neighbor_qp);
      break;
    case Moose::NeighborElement:
      res = -_test_neighbor(neighbor_datum, i, neighbor_qp) * coefficient *
            shiftedPhi(_phi, _grad_phi, elem_datum, distance, j, qp);
      break;
    case Moose::NeighborNeighbor:
      res = _test_neighbor(neighbor_datum, i, neighbor_qp) * coefficient *
            shiftedPhi(_phi_neighbor, _grad_phi_neighbor, neighbor_datum, distance, j, neighbor_qp);
      break;
  }

  return res;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeDResidualDDeformationGradient(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum) const
{
  const bool trial_on_neighbor =
      (type == Moose::ElementNeighbor || type == Moose::NeighborNeighbor);
  const auto grad_phi = trial_on_neighbor ? _grad_phi_neighbor(neighbor_datum, j, neighbor_qp)
                                          : _grad_phi(elem_datum, j, qp);

  Real derivative = 0.0;
  const auto dPK1_dF = _dPK1traction_dF(elem_datum, qp);
  for (unsigned int k = 0; k < _ndisp; ++k)
    derivative += dPK1_dF(_component, component_j, k) * 0.5 * grad_phi(k);

  if (_shifted)
    derivative *= areaCorrection(elem_datum, qp);

  const bool residual_on_element =
      (type == Moose::ElementElement || type == Moose::ElementNeighbor);
  return derivative * (residual_on_element ? -_test(elem_datum, i, qp)
                                           : _test_neighbor(neighbor_datum, i, neighbor_qp));
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeDirectionalCorrectionJacobian(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const Moose::Kokkos::Real3 & avg_grad_phi) const
{
  if (type == Moose::ElementNeighbor || type == Moose::NeighborElement)
    return 0.0;

  const auto normal_tangent = normalTangent(elem_datum, qp);

  Real test;
  Moose::Kokkos::Real3 grad_phi;
  Real part = 0.0;
  if (type == Moose::ElementElement)
  {
    test = _test(elem_datum, i, qp);
    grad_phi = _grad_phi(elem_datum, j, qp);
    const auto tangent = _tangent(elem_datum, qp);
    for (unsigned int jj = 0; jj < _ndisp; ++jj)
      for (unsigned int ll = 0; ll < _ndisp; ++ll)
        part += normal_tangent(jj) * tangent(_component, jj, component_j, ll) * grad_phi(ll);
  }
  else
  {
    test = _test_neighbor(neighbor_datum, i, neighbor_qp);
    grad_phi = _grad_phi_neighbor(neighbor_datum, j, neighbor_qp);
    const auto tangent = _tangent_neighbor(neighbor_datum, neighbor_qp);
    for (unsigned int jj = 0; jj < _ndisp; ++jj)
      for (unsigned int ll = 0; ll < _ndisp; ++ll)
        part += normal_tangent(jj) * tangent(_component, jj, component_j, ll) * grad_phi(ll);
  }

  if (_volumetric_locking_correction)
  {
    const bool on_element = type == Moose::ElementElement;
    part += computeFbarDirectionalCorrection(on_element,
                                             component_j,
                                             on_element ? qp : neighbor_qp,
                                             on_element ? elem_datum : neighbor_datum,
                                             normal_tangent,
                                             grad_phi,
                                             avg_grad_phi);
  }

  const Real correction_jacobian = part * test;

  return type == Moose::ElementElement ? -correction_jacobian : correction_jacobian;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeFbarDirectionalCorrection(
    const bool on_element,
    const unsigned int component_j,
    const unsigned int row_qp,
    AssemblyDatum & row_datum,
    const Moose::Kokkos::Real3 & normal_tangent,
    const Moose::Kokkos::Real3 & grad_phi,
    const Moose::Kokkos::Real3 & avg_grad_phi) const
{
  const auto stabilized_tangent = on_element ? _stabilized_tangent(row_datum, row_qp)
                                             : _stabilized_tangent_neighbor(row_datum, row_qp);
  const auto physical = on_element ? _physical_deformation_gradient(row_datum, row_qp)
                                   : _physical_deformation_gradient_neighbor(row_datum, row_qp);
  const auto average = on_element ? _average_deformation_gradient(row_datum, row_qp)
                                  : _average_deformation_gradient_neighbor(row_datum, row_qp);

  Moose::Kokkos::Real33 U, M;
  for (unsigned int k = 0; k < 3; ++k)
    for (unsigned int L = 0; L < 3; ++L)
    {
      U(k, L) = physical(k, L);
      M(k, L) = average(k, L);
    }
  const Real gamma = ::Kokkos::cbrt(M.determinant() / U.determinant());
  const auto U_inv = U.inverse();
  const auto M_inv = M.inverse();

  // For the trial direction e_c (x) grad(phi_j) with c = component_j, U^{-T} : dU and M^{-T} : dM
  // reduce to the c-th column of the inverses contracted with the local and element-volume
  // averaged trial gradients.
  Real U_invT_dU = 0.0;
  Real M_invT_dM = 0.0;
  for (unsigned int L = 0; L < 3; ++L)
  {
    U_invT_dU += U_inv(L, component_j) * grad_phi(L);
    M_invT_dM += M_inv(L, component_j) * avg_grad_phi(L);
  }

  // With B = gamma U and dgamma = gamma / 3 (M^{-T} : dM - U^{-T} : dU), the B path of dP is
  // C_B : dB = gamma C_B : dU + Q dgamma, where Q = C_B : U.
  Real correction = 0.0;
  for (unsigned int J = 0; J < 3; ++J)
  {
    Real Q = 0.0;
    Real stabilized_tangent_dU = 0.0;
    for (unsigned int L = 0; L < 3; ++L)
    {
      stabilized_tangent_dU += stabilized_tangent(_component, J, component_j, L) * grad_phi(L);
      for (unsigned int k = 0; k < 3; ++k)
        Q += stabilized_tangent(_component, J, k, L) * U(k, L);
    }
    correction += normal_tangent(J) *
                  (gamma * stabilized_tangent_dU + gamma / 3.0 * Q * (M_invT_dM - U_invT_dU));
  }

  return correction;
}
