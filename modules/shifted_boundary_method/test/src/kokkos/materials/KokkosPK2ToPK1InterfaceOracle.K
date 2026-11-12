//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosPK2ToPK1InterfaceOracle.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodTestApp",
                                KokkosPK2ToPK1PrimaryInterfaceOracle);
registerKokkosInterfaceMaterial("ShiftedBoundaryMethodTestApp",
                                KokkosPK2ToPK1NeighborInterfaceOracle);

template <bool neighbor>
InputParameters
KokkosPK2ToPK1InterfaceOracle<neighbor>::validParams()
{
  auto params = InterfaceMaterial::validParams();
  if constexpr (neighbor)
    params.set<Moose::MaterialDataType>("_material_data_type") = Moose::NEIGHBOR_MATERIAL_DATA;
  params.addRequiredCoupledVar("displacements", "Displacement variables in Cartesian order.");
  params.addRequiredParam<MaterialPropertyName>("pk2_stress", "Full PK2 stress property.");
  params.addRequiredParam<MaterialPropertyName>("dpk2_dF",
                                                "Full PK2 deformation-gradient tangent.");
  params.addRequiredParam<MaterialPropertyName>("pk1_stress", "Converted PK1 stress property.");
  params.addRequiredParam<MaterialPropertyName>("dpk1_dF",
                                                "Converted PK1 deformation-gradient tangent.");
  params.addClassDescription(
      "Checks every component of the Kokkos interface PK2-to-PK1 conversion on one side.");
  return params;
}

template <bool neighbor>
KokkosPK2ToPK1InterfaceOracle<neighbor>::KokkosPK2ToPK1InterfaceOracle(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _grad_displacements(kokkosCoupledGradients("displacements")),
    _pk2_stress(
        getKokkosMaterialPropertyByName<Real, 2>(getParam<MaterialPropertyName>("pk2_stress"))),
    _dpk2_dF(getKokkosMaterialPropertyByName<Real, 4>(getParam<MaterialPropertyName>("dpk2_dF"))),
    _pk1_stress(
        getKokkosMaterialPropertyByName<Real, 2>(getParam<MaterialPropertyName>("pk1_stress"))),
    _dpk1_dF(getKokkosMaterialPropertyByName<Real, 4>(getParam<MaterialPropertyName>("dpk1_dF")))
{
}

template class KokkosPK2ToPK1InterfaceOracle<false>;
template class KokkosPK2ToPK1InterfaceOracle<true>;
