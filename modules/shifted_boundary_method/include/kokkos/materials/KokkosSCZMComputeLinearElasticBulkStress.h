//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceMaterial.h"

/**
 * Computes the isotropic linear-elastic bulk Cauchy stress and Lame constants of both physical
 * sides of a shifted cohesive-zone interface.
 *
 * This material supplies the bulk response consumed by the Kokkos SCZM directional correction.
 * The primary and neighbor sides carry independently configurable elastic constants so that
 * heterogeneous interfaces are representable, and all four properties are written through the
 * primary face quadrature point address because the interface material is the single writer of an
 * oriented face.
 *
 * Only the CARTESIAN coordinate system is supported. In curvilinear coordinates the bulk strain
 * must include hoop terms, which this compact producer does not compute.
 */
class KokkosSCZMComputeLinearElasticBulkStress : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosSCZMComputeLinearElasticBulkStress(const InputParameters & parameters);
  KokkosSCZMComputeLinearElasticBulkStress(const KokkosSCZMComputeLinearElasticBulkStress & object);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & /*face*/,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           unsigned int qp,
                                           unsigned int neighbor_qp) const
  {
    const auto stress = bulkStress(gradients(elem_datum, qp), _youngs_modulus, _poissons_ratio);
    const auto stress_neighbor = bulkStress(
        gradients(neighbor_datum, neighbor_qp), _neighbor_youngs_modulus, _neighbor_poissons_ratio);

    // The interface material is the single writer of an oriented face, so both physical sides are
    // written through the primary face quadrature point address. The kernel reads them the same
    // way.
    auto primary_stress = _sczm_bulk_stress(elem_datum, qp);
    auto neighbor_stress = _sczm_bulk_stress_neighbor(elem_datum, qp);
    auto primary_lame = _sczm_bulk_lame(elem_datum, qp);
    auto neighbor_lame = _sczm_bulk_lame_neighbor(elem_datum, qp);

    for (const auto i : make_range(3))
      for (const auto j : make_range(3))
      {
        primary_stress(i, j) = stress(i, j);
        neighbor_stress(i, j) = stress_neighbor(i, j);
      }

    primary_lame(0) = lameLambda(_youngs_modulus, _poissons_ratio);
    primary_lame(1) = lameMu(_youngs_modulus, _poissons_ratio);
    neighbor_lame(0) = lameLambda(_neighbor_youngs_modulus, _neighbor_poissons_ratio);
    neighbor_lame(1) = lameMu(_neighbor_youngs_modulus, _neighbor_poissons_ratio);
  }

private:
  /// Builds a zeroed 3-by-3 displacement gradient holding the active rows and columns
  KOKKOS_FUNCTION Moose::Kokkos::Real33 gradients(Datum & datum, unsigned int qp) const
  {
    Moose::Kokkos::Real33 grad_u = 0.0;
    for (const auto i : make_range(_ndisp))
      for (const auto j : make_range(_ndisp))
        grad_u(i, j) = _displacement_gradients(datum, qp, i)(j);
    return grad_u;
  }

  /// Lame first parameter of an isotropic linear elastic material
  static KOKKOS_FUNCTION Real lameLambda(const Real youngs_modulus, const Real poissons_ratio)
  {
    return youngs_modulus * poissons_ratio /
           ((1.0 + poissons_ratio) * (1.0 - 2.0 * poissons_ratio));
  }

  /// Lame shear parameter of an isotropic linear elastic material
  static KOKKOS_FUNCTION Real lameMu(const Real youngs_modulus, const Real poissons_ratio)
  {
    return youngs_modulus / (2.0 * (1.0 + poissons_ratio));
  }

  /**
   * Cauchy stress of the symmetric displacement gradient under isotropic linear elasticity:
   * sigma = lambda * trace(epsilon) * I + 2 * mu * epsilon with epsilon = sym(grad_u).
   */
  static KOKKOS_FUNCTION Moose::Kokkos::Real33 bulkStress(const Moose::Kokkos::Real33 & grad_u,
                                                          const Real youngs_modulus,
                                                          const Real poissons_ratio)
  {
    const auto lambda = lameLambda(youngs_modulus, poissons_ratio);
    const auto mu = lameMu(youngs_modulus, poissons_ratio);

    // Only the active rows and columns of grad_u are meaningful; the caller leaves the rest zero,
    // so the fixed three-component trace below never reads an uninitialized entry.
    const auto trace = grad_u(0, 0) + grad_u(1, 1) + grad_u(2, 2);

    Moose::Kokkos::Real33 stress = 0.0;
    for (const auto i : make_range(3))
      for (const auto j : make_range(3))
      {
        const auto epsilon = 0.5 * (grad_u(i, j) + grad_u(j, i));
        stress(i, j) = lambda * trace * ((i == j) ? 1.0 : 0.0) + 2.0 * mu * epsilon;
      }
    return stress;
  }

  const unsigned int _ndisp;
  const Real _youngs_modulus;
  const Real _poissons_ratio;
  const Real _neighbor_youngs_modulus;
  const Real _neighbor_poissons_ratio;
  const Moose::Kokkos::VariableGradient _displacement_gradients;

  /// Primary side Cauchy stress, dimensions {3, 3}
  Moose::Kokkos::MaterialProperty<Real, 2> _sczm_bulk_stress;
  /// Neighbor side Cauchy stress, dimensions {3, 3}
  Moose::Kokkos::MaterialProperty<Real, 2> _sczm_bulk_stress_neighbor;
  /// Primary side (lambda, mu), dimensions {2}
  Moose::Kokkos::MaterialProperty<Real, 1> _sczm_bulk_lame;
  /// Neighbor side (lambda, mu), dimensions {2}
  Moose::Kokkos::MaterialProperty<Real, 1> _sczm_bulk_lame_neighbor;
};
