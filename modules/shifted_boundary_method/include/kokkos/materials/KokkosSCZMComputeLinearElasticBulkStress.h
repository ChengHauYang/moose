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
 * Computes the isotropic linear-elastic bulk Cauchy stress and Lame constants on both physical
 * sides of a shifted cohesive-zone interface.
 *
 * This material supplies the bulk response consumed by the directional correction of
 * KokkosSCZMInterfaceKernelSmallStrain. The primary and neighbor sides carry independently
 * configurable elastic constants so that heterogeneous interfaces are representable, and all four
 * properties are written through the primary face quadrature point address because the interface
 * material is the single writer of an oriented face.
 *
 * The directional projection itself is coordinate-system independent. The bulk strain is not:
 * curvilinear coordinate systems contribute an out-of-plane strain term that the plain symmetric
 * displacement gradient omits. The formulas reused here are the host solid-mechanics small-strain
 * formulas, taken from Compute2DSmallStrain::computeProperties,
 * ComputeAxisymmetricRZSmallStrain::computeOutOfPlaneStrain and
 * ComputeRSphericalSmallStrain::computeProperties.
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
    // The bulk strain is assembled as a full device tensor so that curvilinear out-of-plane terms
    // can be inserted into the diagonal before the symmetric in-plane part is formed.
    const auto strain = bulkStrain(elem_datum, qp);
    const auto strain_neighbor = bulkStrain(neighbor_datum, neighbor_qp);

    const auto stress = cauchyStress(strain, _youngs_modulus, _poissons_ratio);
    const auto stress_neighbor =
        cauchyStress(strain_neighbor, _neighbor_youngs_modulus, _neighbor_poissons_ratio);

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
  /**
   * Lame first parameter of an isotropic linear elastic material.
   */
  static KOKKOS_FUNCTION Real lameLambda(const Real youngs_modulus, const Real poissons_ratio)
  {
    return youngs_modulus * poissons_ratio /
           ((1.0 + poissons_ratio) * (1.0 - 2.0 * poissons_ratio));
  }

  /**
   * Lame shear parameter of an isotropic linear elastic material.
   */
  static KOKKOS_FUNCTION Real lameMu(const Real youngs_modulus, const Real poissons_ratio)
  {
    return youngs_modulus / (2.0 * (1.0 + poissons_ratio));
  }

  /**
   * Cauchy stress sigma = lambda * trace(epsilon) * I + 2 * mu * epsilon.
   *
   * The strain tensor is fully initialized, including the entries that this problem does not use,
   * so that the fixed three-component trace below never reads uninitialized storage.
   */
  static KOKKOS_FUNCTION Moose::Kokkos::Real33 cauchyStress(const Moose::Kokkos::Real33 & strain,
                                                            const Real youngs_modulus,
                                                            const Real poissons_ratio)
  {
    const auto lambda = lameLambda(youngs_modulus, poissons_ratio);
    const auto mu = lameMu(youngs_modulus, poissons_ratio);

    const auto trace = strain(0, 0) + strain(1, 1) + strain(2, 2);

    Moose::Kokkos::Real33 stress = 0.0;
    for (const auto i : make_range(3))
      for (const auto j : make_range(3))
        stress(i, j) = lambda * trace * ((i == j) ? 1.0 : 0.0) + 2.0 * mu * strain(i, j);
    return stress;
  }

  /**
   * The bulk strain of one physical side, including the curvilinear out-of-plane term.
   *
   * In CARTESIAN the plain symmetric gradient is complete. In RZ and RSPHERICAL the out-of-plane
   * hoop strain is radial displacement over radius, which is not obtainable from the displacement
   * gradient of the active components. The value at the coordinate singularity is defined exactly
   * as the host materials define it.
   */
  KOKKOS_FUNCTION Moose::Kokkos::Real33 bulkStrain(Datum & datum, unsigned int qp) const
  {
    Moose::Kokkos::Real33 strain = 0.0;

    for (const auto i : make_range(_ndisp))
      for (const auto j : make_range(_ndisp))
        strain(i, j) = 0.5 * (_displacement_gradients(datum, qp, i)(j) +
                              _displacement_gradients(datum, qp, j)(i));

    if (_coord_sys == Moose::COORD_RZ || _coord_sys == Moose::COORD_RSPHERICAL)
    {
      const auto radius = datum.q_point(qp)(0);
      // Both host materials define the hoop strain as zero on the coordinate axis.
      // Host RZ applies an absolute tolerance of 1e-12 (libMesh::TOLERANCE * libMesh::TOLERANCE)
      // via MooseUtils::absoluteFuzzyEqual, while RSPHERICAL compares radius to 0.0 directly.
      // We reproduce these conventions using GPU-safe operations and guard against division by
      // zero.
      constexpr Real rz_axis_tol =
          1e-12; // Matches libMesh::TOLERANCE * libMesh::TOLERANCE in host RZ material
      const auto on_axis =
          _coord_sys == Moose::COORD_RZ ? (::Kokkos::abs(radius) <= rz_axis_tol) : (radius == 0.0);
      const Real hoop = on_axis ? 0.0 : (_displacements(datum, qp, 0) / radius);

      if (_coord_sys == Moose::COORD_RZ)
        strain(2, 2) = hoop;
      else // COORD_RSPHERICAL: epsilon_theta_theta = epsilon_phi_phi = u_r / r
      {
        strain(1, 1) = hoop;
        strain(2, 2) = hoop;
      }
    }

    return strain;
  }

  const unsigned int _ndisp;
  const Moose::CoordinateSystemType _coord_sys;
  const Real _youngs_modulus;
  const Real _poissons_ratio;
  const Real _neighbor_youngs_modulus;
  const Real _neighbor_poissons_ratio;
  const Moose::Kokkos::VariableValue _displacements;
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
