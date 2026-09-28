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

/// Rotates local cohesive traction and its tangent into global coordinates.
class KokkosCZMComputeGlobalTractionSmallStrain : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosCZMComputeGlobalTractionSmallStrain(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                           Datum & elem_datum,
                                           Datum &,
                                           unsigned int qp,
                                           unsigned int) const
  {
    const auto rotation = _czm_total_rotation(elem_datum, qp);
    const auto local_traction = _interface_traction(elem_datum, qp);
    const auto local_tangent = _dinterface_traction_djump(elem_datum, qp);
    auto global_traction = _traction_global(elem_datum, qp);
    auto global_tangent = _dtraction_djump_global(elem_datum, qp);

    for (const auto i : make_range(_ndisp))
    {
      global_traction(i) = 0;
      for (const auto j : make_range(_ndisp))
        global_traction(i) += rotation(i, j) * local_traction(j);

      for (const auto j : make_range(_ndisp))
      {
        global_tangent(i, j) = 0;
        for (const auto k : make_range(_ndisp))
          for (const auto l : make_range(_ndisp))
            global_tangent(i, j) += rotation(i, k) * local_tangent(k, l) * rotation(j, l);
      }
    }
  }

private:
  const unsigned int _ndisp;
  Moose::Kokkos::MaterialProperty<Real, 2> _czm_total_rotation;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_traction;
  Moose::Kokkos::MaterialProperty<Real, 2> _dinterface_traction_djump;
  Moose::Kokkos::MaterialProperty<Real, 1> _traction_global;
  Moose::Kokkos::MaterialProperty<Real, 2> _dtraction_djump_global;
};
