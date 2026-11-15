//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosShiftedPenaltyInterface.h"

registerKokkosInterfaceKernel("ShiftedBoundaryMethodTestApp", KokkosShiftedPenaltyInterface);

InputParameters
KokkosShiftedPenaltyInterface::validParams()
{
  auto params = ShiftedInterfaceKernel::validParams();
  params.addRequiredParam<Real>("penalty", "Penalty applied to the shifted solution jump.");
  return params;
}

KokkosShiftedPenaltyInterface::KokkosShiftedPenaltyInterface(const InputParameters & parameters)
  : ShiftedInterfaceKernel(parameters), _penalty(getParam<Real>("penalty"))
{
}
