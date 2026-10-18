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

namespace Moose::Kokkos
{

/**
 * Base class for Kokkos interface materials that evaluate properties with automatic
 * differentiation. Derived materials receive AssemblyDatum objects so coupled AD variable
 * values and gradients retain their derivative information in material properties.
 */
class ADInterfaceMaterial : public InterfaceMaterial
{
public:
  using InterfaceMaterial::operator();

  static InputParameters validParams();

  ADInterfaceMaterial(const InputParameters & parameters);
  ADInterfaceMaterial(const ADInterfaceMaterial & object);

  template <typename Derived>
  KOKKOS_FUNCTION void
  operator()(InterfaceCompute, const ThreadID tid, const Derived & material) const;

protected:
  /// AD value of the primary-side interface variable.
  const ADVariableValue _ad_variable_value;
  /// AD value of the neighbor-side interface variable.
  const ADVariableValue _ad_neighbor_variable_value;
};

template <typename Derived>
KOKKOS_FUNCTION void
ADInterfaceMaterial::operator()(InterfaceCompute,
                                const ThreadID tid,
                                const Derived & material) const
{
  const auto & face = _interface_faces[tid];
  auto elem_datum = makePrimaryInterfaceDatum<AssemblyDatum>(face,
                                                             kokkosAssembly(),
                                                             kokkosSystems(),
                                                             _variable_value.variable(),
                                                             _variable_value.variable().var());
  auto neighbor_datum =
      makeNeighborInterfaceDatum<AssemblyDatum>(face,
                                                kokkosAssembly(),
                                                kokkosSystems(),
                                                _neighbor_variable_value.variable(),
                                                _neighbor_variable_value.variable().var());
  elem_datum.do_derivatives(true);
  neighbor_datum.do_derivatives(true);

  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
    material.template computeQpProperties<Derived>(
        face, elem_datum, neighbor_datum, qp, _neighbor_qp_map[face.qp_map_offset + qp]);
}

} // namespace Moose::Kokkos
