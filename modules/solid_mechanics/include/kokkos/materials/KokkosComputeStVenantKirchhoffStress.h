//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosMaterial.h"

/**
 * Kokkos isotropic St. Venant-Kirchhoff model: the PK2 stress S = lambda tr(E) I + 2 mu E of the
 * Green-Lagrange strain E = (F^T F - I) / 2 and its derivative with respect to the deformation
 * gradient F, as inputs for KokkosComputeLagrangianStressCustomPK2. Matches the large kinematics
 * branch of the host ComputeStVenantKirchhoffStress with an isotropic elasticity tensor.
 */
class KokkosComputeStVenantKirchhoffStress : public Moose::Kokkos::Material
{
public:
  static InputParameters validParams();

  KokkosComputeStVenantKirchhoffStress(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const;

private:
  const Real _lambda;
  const Real _mu;
  Moose::Kokkos::MaterialProperty<Real, 2> _deformation_gradient;
  Moose::Kokkos::MaterialProperty<Real, 2> _pk2_stress;
  Moose::Kokkos::MaterialProperty<Real, 4> _dpk2_dF;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosComputeStVenantKirchhoffStress::computeQpProperties(const unsigned int qp,
                                                          Datum & datum) const
{
  const auto F = _deformation_gradient(datum, qp);
  auto S = _pk2_stress(datum, qp);
  auto dS_dF = _dpk2_dF(datum, qp);

  Real E[3][3];
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
    {
      E[i][j] = -0.5 * (i == j);
      for (unsigned int k = 0; k < 3; ++k)
        E[i][j] += 0.5 * F(k, i) * F(k, j);
    }

  const Real trace = E[0][0] + E[1][1] + E[2][2];
  for (unsigned int I = 0; I < 3; ++I)
    for (unsigned int J = 0; J < 3; ++J)
    {
      S(I, J) = _lambda * trace * (I == J) + 2 * _mu * E[I][J];

      // dS_IJ/dF_kL = lambda delta_IJ F_kL + mu (delta_IL F_kJ + F_kI delta_JL)
      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int L = 0; L < 3; ++L)
          dS_dF(I, J, k, L) =
              _lambda * (I == J) * F(k, L) + _mu * ((I == L) * F(k, J) + F(k, I) * (J == L));
    }
}
