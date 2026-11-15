//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosPenaltyInterfaceDiffusion.h"

registerKokkosInterfaceKernel("MooseTestApp", KokkosPenaltyInterfaceDiffusion);

InputParameters
KokkosPenaltyInterfaceDiffusion::validParams()
{
  auto params = InterfaceKernel::validParams();
  params.addRequiredParam<Real>("penalty", "Penalty applied to the solution jump.");
  params.addRequiredCoupledVar("primary_coupled_var", "Coupled variable on the primary side.");
  params.addRequiredCoupledVar("neighbor_coupled_var", "Coupled variable on the neighbor side.");
  params.addRequiredParam<MaterialPropertyName>("primary_coefficient",
                                                "Primary-side penalty coefficient.");
  params.addRequiredParam<MaterialPropertyName>("neighbor_coefficient",
                                                "Neighbor-side penalty coefficient.");
  return params;
}

KokkosPenaltyInterfaceDiffusion::KokkosPenaltyInterfaceDiffusion(
    const InputParameters & parameters)
  : InterfaceKernel(parameters),
    _penalty(getParam<Real>("penalty")),
    _primary_coupled_value(kokkosCoupledValue("primary_coupled_var")),
    _neighbor_coupled_value(kokkosCoupledValue("neighbor_coupled_var")),
    _primary_coupled_var(coupled("primary_coupled_var")),
    _neighbor_coupled_var(coupled("neighbor_coupled_var")),
    _primary_coefficient(getKokkosMaterialProperty<Real>("primary_coefficient")),
    _neighbor_coefficient(getKokkosNeighborMaterialProperty<Real>("neighbor_coefficient"))
{
}
