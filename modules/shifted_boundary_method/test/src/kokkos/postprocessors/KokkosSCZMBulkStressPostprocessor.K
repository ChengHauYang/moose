//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSCZMBulkStressPostprocessor.h"

registerKokkosUserObject("ShiftedBoundaryMethodTestApp", KokkosSCZMBulkStressPostprocessor);

InputParameters
KokkosSCZMBulkStressPostprocessor::validParams()
{
  InputParameters params = KokkosSideIntegralPostprocessor::validParams();
  params.addParam<MaterialPropertyName>(
      "stress_property", "sczm_bulk_stress", "The name of the bulk stress material property.");
  params.addParam<unsigned int>("component_i", 0, "The first stress tensor index (0, 1, or 2).");
  params.addParam<unsigned int>("component_j", 0, "The second stress tensor index (0, 1, or 2).");
  params.set<bool>("average") = true;
  params.suppressParameter<bool>("average");
  params.addClassDescription(
      "Test postprocessor that computes the side average of a bulk stress tensor component.");
  return params;
}

KokkosSCZMBulkStressPostprocessor::KokkosSCZMBulkStressPostprocessor(
    const InputParameters & parameters)
  : KokkosSideIntegralPostprocessor(parameters),
    _component_i(getParam<unsigned int>("component_i")),
    _component_j(getParam<unsigned int>("component_j")),
    _stress(
        getKokkosMaterialPropertyByName<Real, 2>(getParam<MaterialPropertyName>("stress_property")))
{
}

KokkosSCZMBulkStressPostprocessor::KokkosSCZMBulkStressPostprocessor(
    const KokkosSCZMBulkStressPostprocessor & object)
  : KokkosSideIntegralPostprocessor(object),
    _component_i(object._component_i),
    _component_j(object._component_j),
    _stress(object._stress)
{
}
