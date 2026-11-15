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

/// Computes a pure elastic local cohesive traction-separation law.
class KokkosPureElasticTractionSeparation : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosPureElasticTractionSeparation(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                           Datum & elem_datum,
                                           Datum &,
                                           unsigned int qp,
                                           unsigned int) const
  {
    const auto jump = _interface_displacement_jump(elem_datum, qp);
    auto traction = _interface_traction(elem_datum, qp);
    auto tangent = _dinterface_traction_djump(elem_datum, qp);

    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      const auto stiffness = i == 0 ? _normal_stiffness : _tangent_stiffness;
      traction(i) = stiffness * jump(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
        tangent(i, j) = i == j ? stiffness : 0;
    }
  }

private:
  const unsigned int _ndisp;
  const Real _normal_stiffness;
  const Real _tangent_stiffness;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_displacement_jump;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_traction;
  Moose::Kokkos::MaterialProperty<Real, 2> _dinterface_traction_djump;
};
