//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosADInterfaceMaterial.h"

namespace Moose::Kokkos
{

InputParameters
ADInterfaceMaterial::validParams()
{
  return InterfaceMaterial::validParams();
}

ADInterfaceMaterial::ADInterfaceMaterial(const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ad_variable_value(isCoupled("variable") ? kokkosADCoupledValue("variable")
                                             : ADVariableValue()),
    _ad_neighbor_variable_value(isCoupled("neighbor_var") ? kokkosADCoupledValue("neighbor_var")
                                                          : ADVariableValue())
{
}

ADInterfaceMaterial::ADInterfaceMaterial(const ADInterfaceMaterial & object)
  : InterfaceMaterial(object),
    _ad_variable_value(object._ad_variable_value),
    _ad_neighbor_variable_value(object._ad_neighbor_variable_value)
{
}

} // namespace Moose::Kokkos
