//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosNEML2InterfaceRetrieverOracle.h"

registerKokkosInterfaceMaterial("MooseTestApp", KokkosNEML2PrimaryInterfaceRetrieverOracle);
registerKokkosInterfaceMaterial("MooseTestApp", KokkosNEML2NeighborInterfaceRetrieverOracle);

template <bool neighbor>
InputParameters
KokkosNEML2InterfaceRetrieverOracle<neighbor>::validParams()
{
  auto params = InterfaceMaterial::validParams();
  if constexpr (neighbor)
    params.set<Moose::MaterialDataType>("_material_data_type") = Moose::NEIGHBOR_MATERIAL_DATA;
  params.addRequiredParam<Real>("expected_normal_stress", "Expected xx stress component.");
  params.addRequiredParam<Real>("expected_transverse_stress", "Expected yy stress component.");
  params.addRequiredParam<Real>("expected_shear_stress", "Expected xy stress component.");
  params.addRequiredParam<Real>("expected_shear_tangent", "Expected xyxy tangent component.");
  params.addRequiredParam<MaterialPropertyName>("stress", "Retrieved symmetric rank-two property.");
  params.addRequiredParam<MaterialPropertyName>("tangent",
                                                "Retrieved symmetric rank-four property.");
  params.addClassDescription(
      "Checks NEML2 Kokkos interface Retriever values on one property side.");
  return params;
}

template <bool neighbor>
KokkosNEML2InterfaceRetrieverOracle<neighbor>::KokkosNEML2InterfaceRetrieverOracle(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _expected_normal_stress(getParam<Real>("expected_normal_stress")),
    _expected_transverse_stress(getParam<Real>("expected_transverse_stress")),
    _expected_shear_stress(getParam<Real>("expected_shear_stress")),
    _expected_shear_tangent(getParam<Real>("expected_shear_tangent")),
    _stress(getKokkosMaterialPropertyByName<Real, 2>(getParam<MaterialPropertyName>("stress"))),
    _tangent(getKokkosMaterialPropertyByName<Real, 4>(getParam<MaterialPropertyName>("tangent")))
{
}

template class KokkosNEML2InterfaceRetrieverOracle<false>;
template class KokkosNEML2InterfaceRetrieverOracle<true>;
