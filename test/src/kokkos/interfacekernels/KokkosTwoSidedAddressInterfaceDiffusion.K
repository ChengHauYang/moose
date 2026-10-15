//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosTwoSidedAddressInterfaceDiffusion.h"

registerKokkosInterfaceKernel("MooseTestApp", KokkosTwoSidedAddressInterfaceDiffusion);

InputParameters
KokkosTwoSidedAddressInterfaceDiffusion::validParams()
{
  auto params = InterfaceKernel::validParams();
  params.addRequiredCoupledVar("primary_coupled_var", "Primary-side interface variable.");
  params.addRequiredCoupledVar("neighbor_coupled_var", "Neighbor-side interface variable.");
  params.addRequiredParam<MaterialPropertyName>("side_address",
                                                "Primary-side address from boundary storage.");
  params.addRequiredParam<MaterialPropertyName>("neighbor_address",
                                                "Neighbor-side address from neighbor storage.");
  params.addRequiredParam<MaterialPropertyName>("dside_address_djump",
                                                "Derivative of the primary-side address.");
  params.addRequiredParam<MaterialPropertyName>("dneighbor_address_djump",
                                                "Derivative of the neighbor-side address.");
  return params;
}

KokkosTwoSidedAddressInterfaceDiffusion::KokkosTwoSidedAddressInterfaceDiffusion(
    const InputParameters & parameters)
  : InterfaceKernel(parameters),
    _side_address(getKokkosMaterialPropertyByName<Real>("side_address")),
    _neighbor_address(getKokkosNeighborMaterialPropertyByName<Real>("neighbor_address")),
    _dside_address_djump(getKokkosMaterialPropertyByName<Real>("dside_address_djump")),
    _dneighbor_address_djump(
        getKokkosNeighborMaterialPropertyByName<Real>("dneighbor_address_djump"))
{
}
