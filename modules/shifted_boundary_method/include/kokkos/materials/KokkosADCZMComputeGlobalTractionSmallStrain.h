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

/// Rotates AD local cohesive traction into global coordinates.
class KokkosADCZMComputeGlobalTractionSmallStrain : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosADCZMComputeGlobalTractionSmallStrain(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                           Datum & elem_datum,
                                           Datum &,
                                           unsigned int qp,
                                           unsigned int) const
  {
    const auto rotation = _czm_total_rotation(elem_datum, qp);
    const auto local_traction = _interface_traction(elem_datum, qp);
    auto global_traction = _traction_global(elem_datum, qp);

    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      global_traction(i) = 0;
      for (unsigned int j = 0; j < _ndisp; ++j)
        global_traction(i) += rotation(i, j) * local_traction(j);
    }
  }

private:
  const unsigned int _ndisp;
  const Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
  const Moose::Kokkos::MaterialProperty<Moose::Kokkos::ADReal, 1> _interface_traction;
  Moose::Kokkos::MaterialProperty<Moose::Kokkos::ADReal, 1> _traction_global;
};
