//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosLinearCohesiveZoneMaterial.h"

registerKokkosMaterial("ShiftedBoundaryMethodTestApp", KokkosLinearCohesiveZoneMaterial);

InputParameters
KokkosLinearCohesiveZoneMaterial::validParams()
{
  auto params = Material::validParams();
  params.addRequiredParam<unsigned int>("ndisp", "Number of displacement components.");
  params.addRequiredParam<std::vector<Real>>(
      "stiffness", "Row-major entries of the cohesive tangent matrix.");
  params.addParam<std::string>("base_name", "Material property base name.");
  return params;
}

KokkosLinearCohesiveZoneMaterial::KokkosLinearCohesiveZoneMaterial(
    const InputParameters & parameters)
  : Material(parameters), _ndisp(getParam<unsigned int>("ndisp"))
{
  if (_ndisp < 1 || _ndisp > 3)
    paramError("ndisp", "One, two, or three displacement components are required.");

  const auto & stiffness = getParam<std::vector<Real>>("stiffness");
  if (stiffness.size() != _ndisp * _ndisp)
    paramError("stiffness", "Expected ndisp * ndisp entries.");

  _stiffness.create(_ndisp, _ndisp);
  for (unsigned int i = 0; i < _ndisp; ++i)
    for (unsigned int j = 0; j < _ndisp; ++j)
      _stiffness(i, j) = stiffness[i * _ndisp + j];
  _stiffness.copyToDevice();

  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _traction_global =
      declareKokkosPropertyByName<Real, 1>(prefix + "traction_global", {_ndisp});
  _dtraction_djump_global = declareKokkosPropertyByName<Real, 2>(
      prefix + "dtraction_djump_global", {_ndisp, _ndisp});
}
