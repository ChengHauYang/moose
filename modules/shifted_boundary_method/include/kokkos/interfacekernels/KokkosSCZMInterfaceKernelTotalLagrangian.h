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
#include "KokkosSCZMStabilizationData.h"
#include "SCZMCohesiveLawEvaluators.h"
#include "SCZMGeneralizedRobinSolver.h"
#include "SCZMGlobalTractionTotalLagrangian.h"
#include "SCZMWeightedTractionStabilization.h"

/**
 * Kokkos shifted cohesive-zone interface kernel for total-Lagrangian finite-strain displacements.
 */
class KokkosSCZMInterfaceKernelTotalLagrangian : public Moose::Kokkos::ShiftedInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosSCZMInterfaceKernelTotalLagrangian(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void residualSetup() override;
  virtual void jacobianSetup() override;
  virtual void computeResidual() override;
  virtual void computeJacobian() override;

  using Moose::Kokkos::InterfaceKernel::operator();

  KOKKOS_FUNCTION bool useVolumeResidual() const { return _element_linear_recovery; }
  KOKKOS_FUNCTION bool useVolumeJacobian() const
  {
    return _element_linear_recovery || _volumetric_locking_correction;
  }

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpResidual(Moose::DGResidualType type,
                                         unsigned int i,
                                         unsigned int qp,
                                         unsigned int neighbor_qp,
                                         AssemblyDatum & elem_datum,
                                         AssemblyDatum & neighbor_datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real computeQpResidualWithVolume(Moose::DGResidualType type,
                                                   unsigned int i,
                                                   unsigned int qp,
                                                   unsigned int neighbor_qp,
                                                   AssemblyDatum & elem_datum,
                                                   AssemblyDatum & neighbor_datum,
                                                   AssemblyDatum & elem_volume_datum,
                                                   AssemblyDatum & neighbor_volume_datum) const;

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
  KOKKOS_FUNCTION JacobianQpData prepareJacobianQp(Moose::DGJacobianType type,
                                                   unsigned int j,
                                                   unsigned int qp,
                                                   unsigned int neighbor_qp,
                                                   AssemblyDatum & elem_datum,
                                                   AssemblyDatum & neighbor_datum,
                                                   AssemblyDatum & volume_column_datum,
                                                   const Moose::Kokkos::Real3 & avg_grad_phi) const;

  template <typename Derived>
  KOKKOS_FUNCTION Real
  computeQpJacobianWithColumn(Moose::DGJacobianType type,
                              unsigned int i,
                              unsigned int j,
                              unsigned int qp,
                              unsigned int neighbor_qp,
                              AssemblyDatum & elem_datum,
                              AssemblyDatum & neighbor_datum,
                              const Moose::Kokkos::Real3 & avg_grad_phi,
                              const JacobianQpData & recovered_derivatives) const;

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
                                     const Moose::Kokkos::Real3 & avg_grad_phi,
                                     const JacobianQpData & recovered_derivatives) const;

protected:
  void prepareStabilization(bool need_tangents);

  KOKKOS_FUNCTION dof_id_type recoveryOffset(AssemblyDatum & elem_datum,
                                             unsigned int qp,
                                             bool on_element,
                                             bool at_surrogate = false) const
  {
    return _recovery_offsets[4 * (_face_qp_offsets(elem_datum.side(), elem_datum.elemID()) + qp) +
                             2 * at_surrogate + (on_element ? 0 : 1)];
  }

  KOKKOS_FUNCTION Real recoveredStress(unsigned int row,
                                       unsigned int column,
                                       unsigned int qp,
                                       bool on_element,
                                       AssemblyDatum & elem_datum,
                                       AssemblyDatum & volume_datum,
                                       bool at_surrogate = false) const;

  /// Jacobian of this component's residual with respect to displacement component component_j
  KOKKOS_FUNCTION Real computeJacobianComponent(Moose::DGJacobianType type,
                                                unsigned int i,
                                                unsigned int j,
                                                unsigned int component_j,
                                                unsigned int qp,
                                                unsigned int neighbor_qp,
                                                AssemblyDatum & elem_datum,
                                                AssemblyDatum & neighbor_datum,
                                                const Moose::Kokkos::Real3 & avg_grad_phi,
                                                const JacobianQpData & recovered_derivatives) const;

  /**
   * Derivatives of the residual through the cohesive traction component traction_component with
   * respect to displacement component component_j, through the displacement jump and through the
   * deformation gradient, including test function, sign, and area correction.
   */
  ///@{
  KOKKOS_FUNCTION Real computeDResidualDDisplacement(Moose::DGJacobianType type,
                                                     unsigned int i,
                                                     unsigned int j,
                                                     unsigned int traction_component,
                                                     unsigned int component_j,
                                                     unsigned int qp,
                                                     unsigned int neighbor_qp,
                                                     AssemblyDatum & elem_datum,
                                                     AssemblyDatum & neighbor_datum,
                                                     Real tangent) const;

  KOKKOS_FUNCTION Real computeDResidualDDeformationGradient(Moose::DGJacobianType type,
                                                            unsigned int i,
                                                            unsigned int j,
                                                            unsigned int traction_component,
                                                            unsigned int component_j,
                                                            unsigned int qp,
                                                            unsigned int neighbor_qp,
                                                            AssemblyDatum & elem_datum,
                                                            AssemblyDatum & neighbor_datum,
                                                            const Real dtraction_dF[3][3][3]) const;
  ///@}

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
   * Derivative of P.row(stress_row) . direction on the trial side with respect to the trial
   * function of displacement component component_j, including the F-bar terms with
   * volumetric_locking_correction.
   */
  KOKKOS_FUNCTION Real stressDerivative(bool on_element,
                                        unsigned int stress_row,
                                        unsigned int component_j,
                                        unsigned int side_qp,
                                        AssemblyDatum & side_datum,
                                        const Moose::Kokkos::Real3 & direction,
                                        const Moose::Kokkos::Real3 & grad_phi,
                                        const Moose::Kokkos::Real3 & avg_grad_phi) const;

  /** Complete derivative of recovered volume PK1, matching the bulk TL kernel's F-bar chain. */
  KOKKOS_FUNCTION Real volumeStressDerivative(unsigned int stress_row,
                                              unsigned int component_j,
                                              unsigned int volume_qp,
                                              AssemblyDatum & volume_datum,
                                              const Moose::Kokkos::Real3 & direction,
                                              const Moose::Kokkos::Real3 & grad_phi,
                                              const Moose::Kokkos::Real3 & avg_grad_phi) const;

  /**
   * Contribution of the F-bar deformation gradient B to the trial-side derivative
   * n_t . dP / du_j, where P = P(U, B) and B = (det(M) / det(U))^(1/3) U. This adds the
   * local terms gamma C_B - gamma / 3 Q (x) U^{-T} and the element-volume term
   * gamma / 3 Q (x) M^{-T} contracted with the volume-averaged trial gradient avg_grad_phi.
   */
  KOKKOS_FUNCTION Real
  computeFbarDirectionalCorrection(bool on_element,
                                   unsigned int stress_row,
                                   unsigned int component_j,
                                   unsigned int row_qp,
                                   AssemblyDatum & row_datum,
                                   const Moose::Kokkos::Real3 & normal_tangent,
                                   const Moose::Kokkos::Real3 & grad_phi,
                                   const Moose::Kokkos::Real3 & avg_grad_phi) const;

  /// The normal of the stabilization: the true normal when shifted, else the surrogate normal
  KOKKOS_FUNCTION Moose::Kokkos::Real3 stabilizationNormal(AssemblyDatum & datum,
                                                           unsigned int qp) const
  {
    return _shifted ? trueNormal(datum, qp) : datum.normals(qp);
  }

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
  const bool _element_linear_recovery;
  const SCZMWeightedTractionStabilization::Settings _stabilization_settings;
  KokkosSCZMStabilizationData * const _stabilization_data;
  Moose::Kokkos::Array2D<dof_id_type> _face_qp_offsets;
  Moose::Kokkos::Array<Real> _stabilization_beta;
  /// Weight of the primary side stress in the average bulk traction
  Moose::Kokkos::Array<Real> _primary_weight;
  Moose::Kokkos::Array<Moose::Kokkos::Real3> _stabilized_traction;
  Moose::Kokkos::Array<Moose::Kokkos::Real33> _robin_W;
  Moose::Kokkos::Array2D<Real> _robin_weighted_dF;
  Moose::Kokkos::Array<unsigned int> _disp_vars;
  /// Offset for each oriented face, interface QP, target point, and side into the recovery weights
  Moose::Kokkos::Array<dof_id_type> _recovery_offsets;
  Moose::Kokkos::Array<Real> _recovery_weights;
  bool _recovery_initialized = false;
  const Moose::Kokkos::MaterialProperty<Real, 1> _displacement_jump_global;
  const Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
  const Moose::Kokkos::MaterialProperty<Real, 2> _czm_reference_rotation;
  const Moose::Kokkos::MaterialProperty<Real, 2> _czm_rotation;
  const Moose::Kokkos::MaterialProperty<Real, 2> _F_czm;
  const Moose::Kokkos::MaterialProperty<Real, 1> _interface_traction;
  const Moose::Kokkos::MaterialProperty<Real, 2> _dinterface_traction_djump;
  const Moose::Kokkos::MaterialProperty<Real, 1> _traction_global;
  const Moose::Kokkos::MaterialProperty<Real, 2> _dtraction_djump_global;
  const Moose::Kokkos::MaterialProperty<Real, 3> _dPK1traction_dF;
  Moose::Kokkos::MaterialProperty<Real, 2> _stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _tangent;
  Moose::Kokkos::MaterialProperty<Real, 2> _stress_neighbor;
  Moose::Kokkos::MaterialProperty<Real, 4> _tangent_neighbor;
  /// Volume PK1 data used only by element-local recovery
  ///@{
  Moose::Kokkos::MaterialProperty<Real, 2> _volume_stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _volume_tangent;
  Moose::Kokkos::MaterialProperty<Real, 2> _volume_F_avg_invT;
  Moose::Kokkos::MaterialProperty<Real, 2> _volume_fbar_A;
  Moose::Kokkos::MaterialProperty<Real, 0> _volume_fbar_gamma_over_3;
  ///@}

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

  if (!_shifted && !_stabilization_settings.enabled)
    return sign * test * _traction_global(elem_datum, qp)(_component);

  const auto traction = _traction_global(elem_datum, qp);
  Real interface_traction = traction(_component);
  if (_stabilization_settings.enabled)
  {
    const auto index = _face_qp_offsets(elem_datum.side(), elem_datum.elemID()) + qp;
    interface_traction = _stabilized_traction[index](_component);
  }

  const auto residual =
      sign * test * (_shifted ? areaCorrection(elem_datum, qp) : 1.0) * interface_traction;

  if (!_shifted || !_directional_correction)
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
KokkosSCZMInterfaceKernelTotalLagrangian::computeQpResidualWithVolume(
    const Moose::DGResidualType type,
    const unsigned int i,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    AssemblyDatum & elem_volume_datum,
    AssemblyDatum & neighbor_volume_datum) const
{
  if (!_element_linear_recovery)
    return computeQpResidual<Derived>(type, i, qp, neighbor_qp, elem_datum, neighbor_datum);

  const auto test = type == Moose::Element ? _test(elem_datum, i, qp)
                                           : _test_neighbor(neighbor_datum, i, neighbor_qp);
  const auto sign = type == Moose::Element ? -1.0 : 1.0;
  const auto index = _face_qp_offsets(elem_datum.side(), elem_datum.elemID()) + qp;
  const Real interface_traction = _stabilized_traction[index](_component);

  const auto residual = sign * test * areaCorrection(elem_datum, qp) * interface_traction;
  if (!_directional_correction)
    return residual;

  const auto normal_tangent = normalTangent(elem_datum, qp);
  const bool on_element = type == Moose::Element;
  auto & volume_datum = on_element ? elem_volume_datum : neighbor_volume_datum;
  Real directional = 0.0;
  for (unsigned int J = 0; J < _ndisp; ++J)
    directional += recoveredStress(_component, J, qp, on_element, elem_datum, volume_datum, true) *
                   normal_tangent(J);
  return type == Moose::Element ? residual - test * directional : residual + test * directional;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::recoveredStress(const unsigned int row,
                                                          const unsigned int column,
                                                          const unsigned int qp,
                                                          const bool on_element,
                                                          AssemblyDatum & elem_datum,
                                                          AssemblyDatum & volume_datum,
                                                          const bool at_surrogate) const
{
  const auto offset = recoveryOffset(elem_datum, qp, on_element, at_surrogate);
  Real recovered = 0.0;
  for (unsigned int volume_qp = 0; volume_qp < volume_datum.n_qps(); ++volume_qp)
    recovered += _recovery_weights[offset + volume_qp] *
                 _volume_stress(volume_datum, volume_qp)(row, column);
  return recovered;
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
  return computeJacobianComponent(type,
                                  i,
                                  j,
                                  _component,
                                  qp,
                                  neighbor_qp,
                                  elem_datum,
                                  neighbor_datum,
                                  Moose::Kokkos::Real3(0),
                                  {Moose::Kokkos::Real33(0), Moose::Kokkos::Real33(0)});
}

template <typename Derived>
KOKKOS_FUNCTION Moose::Kokkos::Real3
KokkosSCZMInterfaceKernelTotalLagrangian::prepareJacobianColumn(
    const Moose::DGJacobianType type,
    const unsigned int j,
    AssemblyDatum & volume_column_datum) const
{
  // The directional correction differentiates only the same-side stress, while the stabilization
  // also differentiates the counterpart stress in the cross blocks
  const bool cross_block = type == Moose::ElementNeighbor || type == Moose::NeighborElement;
  if (!_volumetric_locking_correction ||
      !(_stabilization_settings.enabled || (_shifted && _directional_correction && !cross_block)))
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
KOKKOS_FUNCTION KokkosSCZMInterfaceKernelTotalLagrangian::JacobianQpData
KokkosSCZMInterfaceKernelTotalLagrangian::prepareJacobianQp(
    const Moose::DGJacobianType type,
    const unsigned int j,
    const unsigned int qp,
    const unsigned int,
    AssemblyDatum & elem_datum,
    AssemblyDatum &,
    AssemblyDatum & volume_column_datum,
    const Moose::Kokkos::Real3 & avg_grad_phi) const
{
  JacobianQpData derivatives{Moose::Kokkos::Real33(0), Moose::Kokkos::Real33(0)};
  if (!_element_linear_recovery)
    return derivatives;

  const bool trial_on_element = type == Moose::ElementElement || type == Moose::NeighborElement;
  unsigned int component_j = _ndisp;
  for (unsigned int component = 0; component < _ndisp; ++component)
    if (_disp_vars[component] == volume_column_datum.jvar())
      component_j = component;
  if (component_j == _ndisp)
    return derivatives;

  const auto true_offset = recoveryOffset(elem_datum, qp, trial_on_element);
  const auto surrogate_offset = recoveryOffset(elem_datum, qp, trial_on_element, true);
  for (unsigned int volume_qp = 0; volume_qp < volume_column_datum.n_qps(); ++volume_qp)
  {
    const auto grad_phi = trial_on_element ? _grad_phi(volume_column_datum, j, volume_qp)
                                           : _grad_phi_neighbor(volume_column_datum, j, volume_qp);
    for (unsigned int row = 0; row < _ndisp; ++row)
      for (unsigned int J = 0; J < _ndisp; ++J)
      {
        Moose::Kokkos::Real3 direction(0);
        direction(J) = 1.0;
        const auto derivative = volumeStressDerivative(
            row, component_j, volume_qp, volume_column_datum, direction, grad_phi, avg_grad_phi);
        derivatives.primary(row, J) += _recovery_weights[true_offset + volume_qp] * derivative;
        derivatives.secondary(row, J) +=
            _recovery_weights[surrogate_offset + volume_qp] * derivative;
      }
  }
  return derivatives;
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
    const Moose::Kokkos::Real3 & avg_grad_phi,
    const JacobianQpData & recovered_derivatives) const
{
  return computeJacobianComponent(type,
                                  i,
                                  j,
                                  _component,
                                  qp,
                                  neighbor_qp,
                                  elem_datum,
                                  neighbor_datum,
                                  avg_grad_phi,
                                  recovered_derivatives);
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
      return computeJacobianComponent(type,
                                      i,
                                      j,
                                      component_j,
                                      qp,
                                      neighbor_qp,
                                      elem_datum,
                                      neighbor_datum,
                                      Moose::Kokkos::Real3(0),
                                      {Moose::Kokkos::Real33(0), Moose::Kokkos::Real33(0)});

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
    const Moose::Kokkos::Real3 & avg_grad_phi,
    const JacobianQpData & recovered_derivatives) const
{
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
                                      avg_grad_phi,
                                      recovered_derivatives);

  return 0;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeJacobianComponent(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const Moose::Kokkos::Real3 & avg_grad_phi,
    const JacobianQpData & recovered_derivatives) const
{
  Real jacobian = 0.0;

  if (_stabilization_settings.enabled)
  {
    const auto normal = stabilizationNormal(elem_datum, qp);
    const auto index = _face_qp_offsets(elem_datum.side(), elem_datum.elemID()) + qp;
    const bool trial_on_element = type == Moose::ElementElement || type == Moose::NeighborElement;
    const auto grad_phi = trial_on_element ? _grad_phi(elem_datum, j, qp)
                                           : _grad_phi_neighbor(neighbor_datum, j, neighbor_qp);
    const Real signed_test = (type == Moose::ElementElement || type == Moose::ElementNeighbor)
                                 ? -_test(elem_datum, i, qp)
                                 : _test_neighbor(neighbor_datum, i, neighbor_qp);

    jacobian += computeDResidualDDisplacement(type,
                                              i,
                                              j,
                                              _component,
                                              component_j,
                                              qp,
                                              neighbor_qp,
                                              elem_datum,
                                              neighbor_datum,
                                              _robin_W[index](_component, component_j));

    Real weighted_dF[3][3][3] = {{{0.0}}};
    for (unsigned int k = 0; k < _ndisp; ++k)
      for (unsigned int l = 0; l < _ndisp; ++l)
        weighted_dF[_component][k][l] = _robin_weighted_dF(index, 9 * _component + 3 * k + l);
    jacobian += computeDResidualDDeformationGradient(type,
                                                     i,
                                                     j,
                                                     _component,
                                                     component_j,
                                                     qp,
                                                     neighbor_qp,
                                                     elem_datum,
                                                     neighbor_datum,
                                                     weighted_dF);

    const Real beta = _stabilization_beta[index];
    // Weight of the trial side stress in the average bulk traction
    const Real trial_weight =
        trial_on_element ? _primary_weight[index] : 1.0 - _primary_weight[index];
    for (unsigned int b = 0; b < _ndisp; ++b)
    {
      Real bulk_derivative = 0.0;
      if (_element_linear_recovery)
        for (unsigned int J = 0; J < _ndisp; ++J)
          bulk_derivative += normal(J) * recovered_derivatives.primary(b, J);
      else
        bulk_derivative = stressDerivative(trial_on_element,
                                           b,
                                           component_j,
                                           trial_on_element ? qp : neighbor_qp,
                                           trial_on_element ? elem_datum : neighbor_datum,
                                           normal,
                                           grad_phi,
                                           avg_grad_phi);
      jacobian += trial_weight * (_shifted ? areaCorrection(elem_datum, qp) : 1.0) * signed_test *
                  _robin_W[index](_component, b) / beta * bulk_derivative;
    }
  }
  else
  {
    const auto tangent = _dtraction_djump_global(elem_datum, qp);
    const auto property_dtraction_dF = _dPK1traction_dF(elem_datum, qp);
    Real dtraction_dF[3][3][3] = {{{0.0}}};
    for (unsigned int a = 0; a < _ndisp; ++a)
      for (unsigned int k = 0; k < _ndisp; ++k)
        for (unsigned int l = 0; l < _ndisp; ++l)
          dtraction_dF[a][k][l] = property_dtraction_dF(a, k, l);
    jacobian = computeDResidualDDisplacement(type,
                                             i,
                                             j,
                                             _component,
                                             component_j,
                                             qp,
                                             neighbor_qp,
                                             elem_datum,
                                             neighbor_datum,
                                             tangent(_component, component_j)) +
               computeDResidualDDeformationGradient(type,
                                                    i,
                                                    j,
                                                    _component,
                                                    component_j,
                                                    qp,
                                                    neighbor_qp,
                                                    elem_datum,
                                                    neighbor_datum,
                                                    dtraction_dF);
  }

  if (_shifted && _directional_correction)
  {
    if (_element_linear_recovery &&
        (type == Moose::ElementElement || type == Moose::NeighborNeighbor))
    {
      const auto normal_tangent = normalTangent(elem_datum, qp);
      const bool on_element = type == Moose::ElementElement;
      const Real test =
          on_element ? _test(elem_datum, i, qp) : _test_neighbor(neighbor_datum, i, neighbor_qp);
      Real derivative = 0.0;
      for (unsigned int J = 0; J < _ndisp; ++J)
        derivative += recovered_derivatives.secondary(_component, J) * normal_tangent(J);
      jacobian += (on_element ? -1.0 : 1.0) * test * derivative;
    }
    else if (!_element_linear_recovery)
      jacobian += computeDirectionalCorrectionJacobian(
          type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum, avg_grad_phi);
  }

  return jacobian;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeDResidualDDisplacement(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int traction_component,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const Real tangent) const
{
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
    const unsigned int traction_component,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum,
    const Real dtraction_dF[3][3][3]) const
{
  const bool trial_on_neighbor =
      (type == Moose::ElementNeighbor || type == Moose::NeighborNeighbor);
  const auto grad_phi = trial_on_neighbor ? _grad_phi_neighbor(neighbor_datum, j, neighbor_qp)
                                          : _grad_phi(elem_datum, j, qp);

  Real derivative = 0.0;
  for (unsigned int k = 0; k < _ndisp; ++k)
    derivative += dtraction_dF[traction_component][component_j][k] * 0.5 * grad_phi(k);

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
KokkosSCZMInterfaceKernelTotalLagrangian::stressDerivative(
    const bool on_element,
    const unsigned int stress_row,
    const unsigned int component_j,
    const unsigned int side_qp,
    AssemblyDatum & side_datum,
    const Moose::Kokkos::Real3 & direction,
    const Moose::Kokkos::Real3 & grad_phi,
    const Moose::Kokkos::Real3 & avg_grad_phi) const
{
  const auto tangent =
      on_element ? _tangent(side_datum, side_qp) : _tangent_neighbor(side_datum, side_qp);

  Real derivative = 0.0;
  for (unsigned int jj = 0; jj < _ndisp; ++jj)
    for (unsigned int ll = 0; ll < _ndisp; ++ll)
      derivative += direction(jj) * tangent(stress_row, jj, component_j, ll) * grad_phi(ll);

  if (_volumetric_locking_correction)
    derivative += computeFbarDirectionalCorrection(on_element,
                                                   stress_row,
                                                   component_j,
                                                   side_qp,
                                                   side_datum,
                                                   direction,
                                                   grad_phi,
                                                   avg_grad_phi);

  return derivative;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::volumeStressDerivative(
    const unsigned int stress_row,
    const unsigned int component_j,
    const unsigned int volume_qp,
    AssemblyDatum & volume_datum,
    const Moose::Kokkos::Real3 & direction,
    const Moose::Kokkos::Real3 & grad_phi,
    const Moose::Kokkos::Real3 & avg_grad_phi) const
{
  const auto tangent = _volume_tangent(volume_datum, volume_qp);
  Real derivative = 0.0;
  for (unsigned int J = 0; J < _ndisp; ++J)
    for (unsigned int L = 0; L < _ndisp; ++L)
      derivative += direction(J) * tangent(stress_row, J, component_j, L) * grad_phi(L);

  if (_volumetric_locking_correction)
  {
    const auto F_avg_invT = _volume_F_avg_invT(volume_datum, 0);
    Real average_contraction = 0.0;
    for (unsigned int L = 0; L < _ndisp; ++L)
      average_contraction += F_avg_invT(component_j, L) * avg_grad_phi(L);

    const auto A = _volume_fbar_A(volume_datum, volume_qp);
    const Real gamma_over_3 = _volume_fbar_gamma_over_3(volume_datum, volume_qp);
    for (unsigned int J = 0; J < _ndisp; ++J)
      derivative += direction(J) * A(stress_row, J) * gamma_over_3 * average_contraction;
  }

  return derivative;
}

KOKKOS_FUNCTION inline Real
KokkosSCZMInterfaceKernelTotalLagrangian::computeFbarDirectionalCorrection(
    const bool on_element,
    const unsigned int stress_row,
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
      stabilized_tangent_dU += stabilized_tangent(stress_row, J, component_j, L) * grad_phi(L);
      for (unsigned int k = 0; k < 3; ++k)
        Q += stabilized_tangent(stress_row, J, k, L) * U(k, L);
    }
    correction += normal_tangent(J) *
                  (gamma * stabilized_tangent_dU + gamma / 3.0 * Q * (M_invT_dM - U_invT_dU));
  }

  return correction;
}
