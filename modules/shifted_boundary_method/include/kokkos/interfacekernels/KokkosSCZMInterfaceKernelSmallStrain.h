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
#include "SCZMWeightedTractionStabilization.h"

/**
 * Kokkos shifted cohesive-zone interface kernel for small-strain displacement variables.
 */
class KokkosSCZMInterfaceKernelSmallStrain : public Moose::Kokkos::ShiftedInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosSCZMInterfaceKernelSmallStrain(const InputParameters & parameters);

  /// prepareJacobianColumn averages trial gradients over the column element's volume quadrature
  /// points for the volumetric locking correction, so it needs a volume datum
  KOKKOS_FUNCTION bool useVolumeJacobian() const { return _volumetric_locking_correction; }

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
                                                   const Moose::Kokkos::Real3 & avg_grad_phi,
                                                   const JacobianQpData & qp_data) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpOffDiagJacobianWithColumn(Moose::DGJacobianType type,
                                                          unsigned int i,
                                                          unsigned int j,
                                                          unsigned int jvar,
                                                          unsigned int qp,
                                                          unsigned int neighbor_qp,
                                                          AssemblyDatum & elem_datum,
                                                          AssemblyDatum & neighbor_datum,
                                                          const Moose::Kokkos::Real3 & avg_grad_phi,
                                                          const JacobianQpData & qp_data) const;

protected:
  /// Jacobian of this component's residual with respect to displacement component component_j
  KOKKOS_FUNCTION Real computeJacobianComponent(Moose::DGJacobianType type,
                                                unsigned int i,
                                                unsigned int j,
                                                unsigned int component_j,
                                                unsigned int qp,
                                                unsigned int neighbor_qp,
                                                AssemblyDatum & elem_datum,
                                                AssemblyDatum & neighbor_datum,
                                                const Moose::Kokkos::Real3 & avg_grad_phi) const;

  /**
   * Derivative of the residual through the cohesive traction component traction_component with
   * respect to displacement component component_j, including test function, sign, and area
   * correction.
   */
  KOKKOS_FUNCTION Real computeDResidualDDisplacement(Moose::DGJacobianType type,
                                                     unsigned int i,
                                                     unsigned int j,
                                                     unsigned int traction_component,
                                                     unsigned int component_j,
                                                     unsigned int qp,
                                                     unsigned int neighbor_qp,
                                                     AssemblyDatum & elem_datum,
                                                     AssemblyDatum & neighbor_datum) const;

  /**
   * The signed directional-correction contribution to one Jacobian block.
   *
   * This reproduces calculateDirectionalCorrectionJacobian of the host
   * SCZMInterfaceKernelSmallStrainTempl<false> together with the signs applied by its callers: the
   * primary tangent feeds ElementElement with a negative sign, the neighbor tangent feeds
   * NeighborNeighbor with a positive sign, and the two cross blocks receive nothing. The cross
   * blocks are zero by host behavior rather than by differentiating the residual.
   *
   * @param i The test node index selecting the natural test value
   * @param j The trial node index selecting the natural trial gradient
   * @param component_j The trial displacement component, which is the fourth tangent index
   */
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
   * Derivative of stress.row(stress_row) . direction on the trial side with respect to the trial
   * function of displacement component component_j, given the natural trial gradient grad_phi and
   * its element-volume average avg_grad_phi used by the volumetric locking correction.
   */
  KOKKOS_FUNCTION Real stressDerivative(bool on_element,
                                        unsigned int stress_row,
                                        unsigned int component_j,
                                        unsigned int side_qp,
                                        AssemblyDatum & side_datum,
                                        const Moose::Kokkos::Real3 & direction,
                                        const Moose::Kokkos::Real3 & grad_phi,
                                        const Moose::Kokkos::Real3 & avg_grad_phi) const;

  /// The normal of the stabilization: the true normal when shifted, else the surrogate normal
  KOKKOS_FUNCTION Moose::Kokkos::Real3 stabilizationNormal(AssemblyDatum & datum,
                                                           unsigned int qp) const
  {
    return _shifted ? trueNormal(datum, qp) : datum.normals(qp);
  }

  /// Entry (a, b) of the weighted traction stabilization weight S = beta (K + beta I)^{-1}
  KOKKOS_FUNCTION Real stabilizationWeight(const Moose::Kokkos::Real3 & normal,
                                           unsigned int a,
                                           unsigned int b) const
  {
    return (a == b ? _stabilization_weights.tangential : 0.0) +
           (_stabilization_weights.normal - _stabilization_weights.tangential) * normal(a) *
               normal(b);
  }

  /**
   * The component of the true normal lying in the surrogate face plane:
   * normals - dot(true_normal, normals) * true_normal.
   */
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
  /// Weights of the weighted traction stabilization; disabled by default
  const SCZMWeightedTractionStabilization::Weights _stabilization_weights;
  bool _tangent_is_dpk1_df;
  Moose::Kokkos::Array<unsigned int> _disp_vars;
  const Moose::Kokkos::MaterialProperty<Real, 1> _traction_global;
  const Moose::Kokkos::MaterialProperty<Real, 2> _dtraction_djump_global;
  /// Primary-side stress and its configured constitutive tangent.
  Moose::Kokkos::MaterialProperty<Real, 2> _stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _tangent;
  /// Neighbor-side stress and its configured constitutive tangent.
  Moose::Kokkos::MaterialProperty<Real, 2> _stress_neighbor;
  Moose::Kokkos::MaterialProperty<Real, 4> _tangent_neighbor;
};

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeQpResidual(const Moose::DGResidualType type,
                                                        const unsigned int i,
                                                        const unsigned int qp,
                                                        const unsigned int neighbor_qp,
                                                        AssemblyDatum & elem_datum,
                                                        AssemblyDatum & neighbor_datum) const
{
  const auto test = type == Moose::Element ? _test(elem_datum, i, qp)
                                           : _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto sign = type == Moose::Element ? -1.0 : 1.0;

  // With no_shifted the host falls back to the plain interface residual, which carries neither the
  // area correction nor the directional correction. See computeQpResidual of the host
  // SCZMInterfaceKernelSmallStrainTempl.
  if (!_shifted && !_stabilization_weights.enabled)
    return sign * test * _traction_global(elem_datum, qp)(_component);

  const auto traction = _traction_global(elem_datum, qp);
  Real interface_traction = traction(_component);
  if (_stabilization_weights.enabled)
  {
    // S t_c + (I - S) (stress + stress_neighbor) N / 2, see the host stabilizedTraction
    const auto normal = stabilizationNormal(elem_datum, qp);
    const auto stress = _stress(elem_datum, qp);
    const auto stress_neighbor = _stress_neighbor(neighbor_datum, neighbor_qp);
    interface_traction = 0.0;
    for (unsigned int b = 0; b < _ndisp; ++b)
    {
      Real average_traction = 0.0;
      for (unsigned int J = 0; J < _ndisp; ++J)
        average_traction += 0.5 * (stress(b, J) + stress_neighbor(b, J)) * normal(J);
      const Real weight = stabilizationWeight(normal, _component, b);
      interface_traction +=
          weight * traction(b) + ((b == _component ? 1.0 : 0.0) - weight) * average_traction;
    }
  }

  const auto residual =
      sign * test * (_shifted ? areaCorrection(elem_datum, qp) : 1.0) * interface_traction;

  if (!_shifted || !_directional_correction)
    return residual;

  // Reproduces the directional branch of the host computeQpResidual. The correction is
  // deliberately not scaled by areaCorrection(); that factor applies only to the base cohesive
  // traction term.
  const auto normal_tangent = normalTangent(elem_datum, qp);
  const auto stress = type == Moose::Element ? _stress(elem_datum, qp)
                                             : _stress_neighbor(neighbor_datum, neighbor_qp);

  Real directional = 0.0;
  const unsigned int dim = this->kokkosAssembly().getDimension();
  for (unsigned int j = 0; j < dim; ++j)
    directional += stress(_component, j) * normal_tangent(j);

  return type == Moose::Element ? residual - test * directional : residual + test * directional;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeQpJacobian(const Moose::DGJacobianType type,
                                                        const unsigned int i,
                                                        const unsigned int j,
                                                        const unsigned int qp,
                                                        const unsigned int neighbor_qp,
                                                        AssemblyDatum & elem_datum,
                                                        AssemblyDatum & neighbor_datum) const
{
  return computeJacobianComponent(
      type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum, Moose::Kokkos::Real3(0));
}

template <typename Derived>
KOKKOS_FUNCTION Moose::Kokkos::Real3
KokkosSCZMInterfaceKernelSmallStrain::prepareJacobianColumn(
    const Moose::DGJacobianType type,
    const unsigned int j,
    AssemblyDatum & volume_column_datum) const
{
  // The directional correction differentiates only the same-side stress, while the stabilization
  // also differentiates the counterpart stress in the cross blocks
  const bool cross_block = type == Moose::ElementNeighbor || type == Moose::NeighborElement;
  if (!_volumetric_locking_correction ||
      !(_stabilization_weights.enabled || (_shifted && _directional_correction && !cross_block)))
    return Moose::Kokkos::Real3(0);

  bool is_displacement = false;
  for (unsigned int component = 0; component < _ndisp; ++component)
    is_displacement = is_displacement || _disp_vars[component] == volume_column_datum.jvar();
  if (!is_displacement)
    return Moose::Kokkos::Real3(0);

  // Solution-dependent F-bar averages can be evaluated once per element and reused.
  // avg_grad_phi[j] depends on the Jacobian trial column, so reduce it in the element Jacobian
  // loop. This element-local device reduction requires no host-device communication.
  Moose::Kokkos::Real3 avg_grad_phi(0);
  Real volume = 0.0;
  for (unsigned int volume_qp = 0; volume_qp < volume_column_datum.n_qps(); ++volume_qp)
  {
    const Real weight = volume_column_datum.JxW(volume_qp);
    volume += weight;
    avg_grad_phi += weight * (type == Moose::ElementNeighbor || type == Moose::NeighborNeighbor
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
KokkosSCZMInterfaceKernelSmallStrain::computeQpJacobianWithColumn(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const Moose::Kokkos::Real3 & avg_grad_phi,
    const JacobianQpData &) const
{
  return computeJacobianComponent(
      type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum, avg_grad_phi);
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeQpOffDiagJacobian(const Moose::DGJacobianType type,
                                                               const unsigned int i,
                                                               const unsigned int j,
                                                               const unsigned int jvar,
                                                               const unsigned int qp,
                                                               const unsigned int neighbor_qp,
                                                               AssemblyDatum & elem_datum,
                                                               AssemblyDatum & neighbor_datum) const
{
  // Non-displacement off-diagonal variables receive no correction.
  for (unsigned int component_j = 0; component_j < _ndisp; ++component_j)
    if (_disp_vars[component_j] == jvar)
      return computeJacobianComponent(type,
                                      i,
                                      j,
                                      component_j,
                                      qp,
                                      neighbor_qp,
                                      elem_datum,
                                      neighbor_datum,
                                      Moose::Kokkos::Real3(0));

  return 0;
}

template <typename Derived>
KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeQpOffDiagJacobianWithColumn(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int jvar,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const Moose::Kokkos::Real3 & avg_grad_phi,
    const JacobianQpData &) const
{
  for (unsigned int component_j = 0; component_j < _ndisp; ++component_j)
    if (_disp_vars[component_j] == jvar)
      return computeJacobianComponent(
          type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum, avg_grad_phi);

  return 0;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelSmallStrain::computeJacobianComponent(
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
  Real jacobian = 0.0;

  if (_stabilization_weights.enabled)
  {
    // d/du of S t_c + (I - S) (stress + stress_neighbor) N / 2. Each side's stress depends only on
    // that side's displacements, so the trial side selects the stress, tangent, and trial gradient.
    const auto normal = stabilizationNormal(elem_datum, qp);
    const bool trial_on_element = type == Moose::ElementElement || type == Moose::NeighborElement;
    const auto grad_phi = trial_on_element ? _grad_phi(elem_datum, j, qp)
                                           : _grad_phi_neighbor(neighbor_datum, j, neighbor_qp);
    const Real signed_test = (type == Moose::ElementElement || type == Moose::ElementNeighbor)
                                 ? -_test(elem_datum, i, qp)
                                 : _test_neighbor(neighbor_datum, i, neighbor_qp);

    Real average_traction_derivative = 0.0;
    for (unsigned int b = 0; b < _ndisp; ++b)
    {
      const Real weight = stabilizationWeight(normal, _component, b);
      jacobian +=
          weight * computeDResidualDDisplacement(
                       type, i, j, b, component_j, qp, neighbor_qp, elem_datum, neighbor_datum);
      average_traction_derivative +=
          ((b == _component ? 1.0 : 0.0) - weight) *
          stressDerivative(trial_on_element,
                           b,
                           component_j,
                           trial_on_element ? qp : neighbor_qp,
                           trial_on_element ? elem_datum : neighbor_datum,
                           normal,
                           grad_phi,
                           avg_grad_phi);
    }
    jacobian += 0.5 * (_shifted ? areaCorrection(elem_datum, qp) : 1.0) * signed_test *
                average_traction_derivative;
  }
  else
    jacobian = computeDResidualDDisplacement(
        type, i, j, _component, component_j, qp, neighbor_qp, elem_datum, neighbor_datum);

  if (_shifted && _directional_correction)
    jacobian += computeDirectionalCorrectionJacobian(
        type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum, avg_grad_phi);

  return jacobian;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelSmallStrain::computeDResidualDDisplacement(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int traction_component,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum) const
{
  const auto tangent = _dtraction_djump_global(elem_datum, qp)(traction_component, component_j);

  // With no_shifted the host uses the natural test functions and applies no area correction, see
  // computeDResidualDDisplacement of the host SCZMInterfaceKernelSmallStrainTempl<false>.
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
KokkosSCZMInterfaceKernelSmallStrain::computeDirectionalCorrectionJacobian(
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
  // The host applies no correction to the cross blocks.
  if (type == Moose::ElementNeighbor || type == Moose::NeighborElement)
    return 0.0;

  // The correction Jacobian uses the natural, unshifted test and trial gradient. The shift already
  // enters the base cohesive traction Jacobian through computeDResidualDDisplacement, so applying
  // shiftedPhi() here would double-count it.
  const auto normal_tangent = normalTangent(elem_datum, qp);
  const bool on_element = type == Moose::ElementElement;
  const Real test =
      on_element ? _test(elem_datum, i, qp) : _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto grad_phi = on_element ? _grad_phi(elem_datum, j, qp)
                                   : _grad_phi_neighbor(neighbor_datum, j, neighbor_qp);

  const Real correction_jacobian = stressDerivative(on_element,
                                                    _component,
                                                    component_j,
                                                    on_element ? qp : neighbor_qp,
                                                    on_element ? elem_datum : neighbor_datum,
                                                    normal_tangent,
                                                    grad_phi,
                                                    avg_grad_phi) *
                                   test;

  return on_element ? -correction_jacobian : correction_jacobian;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelSmallStrain::stressDerivative(
    const bool on_element,
    const unsigned int stress_row,
    const unsigned int component_j,
    const unsigned int side_qp,
    AssemblyDatum & side_datum,
    const Moose::Kokkos::Real3 & direction,
    const Moose::Kokkos::Real3 & grad_phi,
    const Moose::Kokkos::Real3 & avg_grad_phi) const
{
  const unsigned int dim = this->kokkosAssembly().getDimension();
  const auto tangent =
      on_element ? _tangent(side_datum, side_qp) : _tangent_neighbor(side_datum, side_qp);

  Real origin_part = 0.0;
  Real symmetric_part = 0.0;
  for (unsigned int jj = 0; jj < dim; ++jj)
    for (unsigned int ll = 0; ll < dim; ++ll)
    {
      origin_part += direction(jj) * tangent(stress_row, jj, component_j, ll) * grad_phi(ll);
      symmetric_part += direction(jj) * tangent(stress_row, jj, ll, component_j) * grad_phi(ll);
    }

  Real derivative = _tangent_is_dpk1_df ? origin_part : 0.5 * (origin_part + symmetric_part);

  if (_volumetric_locking_correction)
  {
    const Real volumetric_increment = (avg_grad_phi(component_j) - grad_phi(component_j)) / 3.0;
    Real tangent_normal_trace = 0.0;
    for (unsigned int m = 0; m < dim; ++m)
      for (unsigned int jj = 0; jj < dim; ++jj)
        tangent_normal_trace += tangent(stress_row, jj, m, m) * direction(jj);
    derivative += tangent_normal_trace * volumetric_increment;
  }

  return derivative;
}
