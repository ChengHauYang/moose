//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosElementExtremeMaterialProperty.h"

registerKokkosUserObject("MooseApp", KokkosElementExtremeMaterialProperty);

InputParameters
KokkosElementExtremeMaterialProperty::validParams()
{
  InputParameters params = KokkosExtremeValueBase<Moose::Kokkos::ElementPostprocessor>::validParams();
  params.addRequiredParam<MaterialPropertyName>("mat_prop",
                                                "Material property for which to find the extreme");
  params.addClassDescription(
      "Determines the minimum or maximum of a material property over a volume.");
  return params;
}

KokkosElementExtremeMaterialProperty::KokkosElementExtremeMaterialProperty(
    const InputParameters & parameters)
  : KokkosExtremeValueBase<Moose::Kokkos::ElementPostprocessor>(parameters),
    _mat_prop(getKokkosMaterialProperty<Real>("mat_prop"))
{
}
