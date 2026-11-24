//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosRankTwoCartesianComponent.h"

registerKokkosMaterial("SolidMechanicsApp", KokkosRankTwoCartesianComponent);

InputParameters
KokkosRankTwoCartesianComponent::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription(
      "Access a component of a rank-two Kokkos material property as a scalar property.");
  params.addRequiredParam<MaterialPropertyName>("rank_two_tensor",
                                                "The rank two material property tensor name");
  params.addRequiredParam<MaterialPropertyName>(
      "property_name", "Name of the material property computed by this model");
  params.addRequiredRangeCheckedParam<unsigned int>(
      "index_i", "index_i >= 0 & index_i <= 2", "The index i of ij for the tensor to output");
  params.addRequiredRangeCheckedParam<unsigned int>(
      "index_j", "index_j >= 0 & index_j <= 2", "The index j of ij for the tensor to output");
  return params;
}

KokkosRankTwoCartesianComponent::KokkosRankTwoCartesianComponent(
    const InputParameters & parameters)
  : Material(parameters),
    _tensor(getKokkosMaterialProperty<Real, 2>("rank_two_tensor")),
    _i(getParam<unsigned int>("index_i")),
    _j(getParam<unsigned int>("index_j")),
    _property(declareKokkosProperty<Real, 0>("property_name"))
{
}
