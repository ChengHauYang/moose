//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosConstantRankTwoMaterial.h"

registerKokkosMaterial("MooseTestApp", KokkosConstantRankTwoMaterial);

InputParameters
KokkosConstantRankTwoMaterial::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription(
      "Declares a full rank-two Kokkos material property with row-major values and an optional "
      "affine spatial scale.");
  params.addRequiredParam<MaterialPropertyName>("prop_name", "Name of the rank-two property");
  params.addRequiredParam<std::vector<Real>>(
      "values", "Nine row-major components (row 0: i=0,j=0..2; row 1: i=1,j=0..2; ...)");
  params.addParam<Real>("x_slope", 0.0, "X slope of the affine tensor scale");
  params.addParam<Real>("y_slope", 0.0, "Y slope of the affine tensor scale");
  return params;
}

KokkosConstantRankTwoMaterial::KokkosConstantRankTwoMaterial(const InputParameters & parameters)
  : Material(parameters),
    _prop(declareKokkosProperty<Real, 2>(getParam<MaterialPropertyName>("prop_name"),
                                         std::vector<unsigned int>{3, 3})),
    _values(9),
    _x_slope(getParam<Real>("x_slope")),
    _y_slope(getParam<Real>("y_slope"))
{
  const auto & v = getParam<std::vector<Real>>("values");
  if (v.size() != 9)
    paramError("values", "Exactly 9 row-major components are required for a 3x3 tensor.");
  for (const auto k : make_range(9))
    _values.hostData()[k] = v[k];
  _values.copyToDevice();
}
