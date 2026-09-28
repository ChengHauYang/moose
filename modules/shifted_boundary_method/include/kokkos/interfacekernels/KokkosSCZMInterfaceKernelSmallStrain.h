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
 * Kokkos shifted cohesive-zone interface kernel for small-strain displacement variables.
 */
class KokkosSCZMInterfaceKernelSmallStrain : public Moose::Kokkos::ShiftedInterfaceKernel
{
public:
  static InputParameters validParams();

  KokkosSCZMInterfaceKernelSmallStrain(const InputParameters & parameters);

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

protected:
  KOKKOS_FUNCTION Real computeDResidualDDisplacement(Moose::DGJacobianType type,
                                                     unsigned int i,
                                                     unsigned int j,
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
  KOKKOS_FUNCTION Real computeDirectionalCorrectionJacobian(Moose::DGJacobianType type,
                                                            unsigned int i,
                                                            unsigned int j,
                                                            unsigned int component_j,
                                                            unsigned int qp,
                                                            unsigned int neighbor_qp,
                                                            AssemblyDatum & elem_datum,
                                                            AssemblyDatum & neighbor_datum) const;

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

  /**
   * The isotropic linear elastic tangent component
   * C(a,b,c,d) = lambda * delta(a,b) * delta(c,d)
   *            + mu * (delta(a,c) * delta(b,d) + delta(a,d) * delta(b,c)).
   *
   * Returned directly so that no rank-four tensor is allocated in device code.
   */
  static KOKKOS_FUNCTION Real isotropicElasticComponent(const Real lambda,
                                                        const Real mu,
                                                        unsigned int a,
                                                        unsigned int b,
                                                        unsigned int c,
                                                        unsigned int d)
  {
    const Real dab = (a == b) ? 1.0 : 0.0;
    const Real dcd = (c == d) ? 1.0 : 0.0;
    const Real dac = (a == c) ? 1.0 : 0.0;
    const Real dbd = (b == d) ? 1.0 : 0.0;
    const Real dad = (a == d) ? 1.0 : 0.0;
    const Real dbc = (b == c) ? 1.0 : 0.0;
    return lambda * dab * dcd + mu * (dac * dbd + dad * dbc);
  }

  const unsigned int _component;
  const unsigned int _ndisp;
  const bool _shifted;
  const bool _directional_correction;
  Moose::Kokkos::Array<unsigned int> _disp_vars;
  const Moose::Kokkos::MaterialProperty<Real, 1> _traction_global;
  const Moose::Kokkos::MaterialProperty<Real, 2> _dtraction_djump_global;
  /// Primary side Cauchy stress, dimensions {3, 3}
  Moose::Kokkos::MaterialProperty<Real, 2> _sczm_bulk_stress;
  /// Neighbor side Cauchy stress, dimensions {3, 3}
  Moose::Kokkos::MaterialProperty<Real, 2> _sczm_bulk_stress_neighbor;
  /// Primary side (lambda, mu), dimensions {2}
  Moose::Kokkos::MaterialProperty<Real, 1> _sczm_bulk_lame;
  /// Neighbor side (lambda, mu), dimensions {2}
  Moose::Kokkos::MaterialProperty<Real, 1> _sczm_bulk_lame_neighbor;
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
  if (!_shifted)
    return sign * test * _traction_global(elem_datum, qp)(_component);

  const auto residual =
      sign * test * areaCorrection(elem_datum, qp) * _traction_global(elem_datum, qp)(_component);

  if (!_directional_correction)
    return residual;

  // Reproduces the directional branch of the host computeQpResidual. The correction is
  // deliberately not scaled by areaCorrection(); that factor applies only to the base cohesive
  // traction term. Both stress properties are stored at the primary face quadrature point address.
  const auto normal_tangent = normalTangent(elem_datum, qp);
  const auto stress = type == Moose::Element ? _sczm_bulk_stress(elem_datum, qp)
                                             : _sczm_bulk_stress_neighbor(elem_datum, qp);

  Real directional = 0.0;
  for (const auto j : make_range(3))
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
  auto jacobian = computeDResidualDDisplacement(
      type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum);

  if (_shifted && _directional_correction)
    jacobian += computeDirectionalCorrectionJacobian(
        type, i, j, _component, qp, neighbor_qp, elem_datum, neighbor_datum);

  return jacobian;
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
    {
      auto jacobian = computeDResidualDDisplacement(
          type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum);

      if (_shifted && _directional_correction)
        jacobian += computeDirectionalCorrectionJacobian(
            type, i, j, component_j, qp, neighbor_qp, elem_datum, neighbor_datum);

      return jacobian;
    }

  return 0;
}

KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeDResidualDDisplacement(
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

  switch (type)
  {
    case Moose::ElementElement:
      return _test(elem_datum, i, qp) * coefficient *
             shiftedPhi(_phi, _grad_phi, elem_datum, distance, j, qp);
    case Moose::ElementNeighbor:
      return -_test(elem_datum, i, qp) * coefficient *
             shiftedPhi(
                 _phi_neighbor, _grad_phi_neighbor, neighbor_datum, distance, j, neighbor_qp);
    case Moose::NeighborElement:
      return -_test_neighbor(neighbor_datum, i, neighbor_qp) * coefficient *
             shiftedPhi(_phi, _grad_phi, elem_datum, distance, j, qp);
    case Moose::NeighborNeighbor:
      return _test_neighbor(neighbor_datum, i, neighbor_qp) * coefficient *
             shiftedPhi(
                 _phi_neighbor, _grad_phi_neighbor, neighbor_datum, distance, j, neighbor_qp);
  }

  return 0;
}

KOKKOS_FUNCTION Real
KokkosSCZMInterfaceKernelSmallStrain::computeDirectionalCorrectionJacobian(
    const Moose::DGJacobianType type,
    const unsigned int i,
    const unsigned int j,
    const unsigned int component_j,
    const unsigned int qp,
    const unsigned int neighbor_qp,
    AssemblyDatum & elem_datum,
    AssemblyDatum & neighbor_datum) const
{
  // The host applies no correction to the cross blocks.
  if (type == Moose::ElementNeighbor || type == Moose::NeighborElement)
    return 0.0;

  // The correction Jacobian uses the natural, unshifted test and trial gradient. The shift already
  // enters the base cohesive traction Jacobian through computeDResidualDDisplacement, so applying
  // shiftedPhi() here would double-count it.
  const auto normal_tangent = normalTangent(elem_datum, qp);

  Real test;
  Moose::Kokkos::Real3 grad_phi;
  Real lambda;
  Real mu;
  if (type == Moose::ElementElement)
  {
    test = _test(elem_datum, i, qp);
    grad_phi = _grad_phi(elem_datum, j, qp);
    const auto lame = _sczm_bulk_lame(elem_datum, qp);
    lambda = lame(0);
    mu = lame(1);
  }
  else
  {
    test = _test_neighbor(neighbor_datum, i, neighbor_qp);
    grad_phi = _grad_phi_neighbor(neighbor_datum, j, neighbor_qp);
    // The neighbor Lame pair is stored at the primary face quadrature point address alongside the
    // primary pair, because the interface material is the single writer of an oriented face.
    const auto lame = _sczm_bulk_lame_neighbor(elem_datum, qp);
    lambda = lame(0);
    mu = lame(1);
  }

  // origin_part    = test * sum_j nt(j) * sum_l C(ivar,j,k,l) * gp(l)
  // symmetric_part = test * sum_j nt(j) * sum_l C(ivar,j,l,k) * gp(l)
  // The first tangent index is the residual displacement component, not the test node index; the
  // test node enters only through the scalar test factor. Both contractions are evaluated even
  // though isotropic major symmetry makes them equal, which preserves the host tangent-definition
  // structure against a future non-isotropic extension.
  Real origin_part = 0.0;
  Real symmetric_part = 0.0;
  for (const auto jj : make_range(3))
    for (const auto ll : make_range(3))
    {
      origin_part += normal_tangent(jj) *
                     isotropicElasticComponent(lambda, mu, _component, jj, component_j, ll) *
                     grad_phi(ll);
      symmetric_part += normal_tangent(jj) *
                        isotropicElasticComponent(lambda, mu, _component, jj, ll, component_j) *
                        grad_phi(ll);
    }

  const auto correction_jacobian = 0.5 * (origin_part + symmetric_part) * test;

  return type == Moose::ElementElement ? -correction_jacobian : correction_jacobian;
}
