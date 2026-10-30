//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosElementPostprocessor.h"
#include "KokkosExtremeValueBase.h"

/**
 * Kokkos counterpart of ElementExtremeMaterialProperty: the per-quadrature-point minimum or maximum
 * of a scalar material property over the domain, matching the host statistic (a raw extremum over
 * every quadrature point, with no volume weighting). getProxyValuePair returns the material-property
 * value in both slots so KokkosExtremeValueBase reduces on it and reports it directly.
 */
class KokkosElementExtremeMaterialProperty
  : public KokkosExtremeValueBase<Moose::Kokkos::ElementPostprocessor>
{
public:
  static InputParameters validParams();

  KokkosElementExtremeMaterialProperty(const InputParameters & parameters);

  KOKKOS_FUNCTION Kokkos::pair<Real, Real> getProxyValuePair(const unsigned int qp,
                                                             Datum & datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION void reduce(Datum & datum, Real * result) const;

protected:
  const Moose::Kokkos::MaterialProperty<Real> _mat_prop;
};

KOKKOS_FUNCTION inline Kokkos::pair<Real, Real>
KokkosElementExtremeMaterialProperty::getProxyValuePair(const unsigned int qp, Datum & datum) const
{
  const auto value = _mat_prop(datum, qp);
  return Kokkos::make_pair(value, value);
}

template <typename Derived>
KOKKOS_FUNCTION void
KokkosElementExtremeMaterialProperty::reduce(Datum & datum, Real * result) const
{
  for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
    static_cast<const Derived *>(this)->template computeExtremeValue<Derived>(qp, datum, result);
}
