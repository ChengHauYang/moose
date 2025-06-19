//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "FunctionPathEllipsoidHeatSource.h"

#include "Function.h"

registerMooseObject("HeatTransferApp", FunctionPathEllipsoidHeatSource);

InputParameters
FunctionPathEllipsoidHeatSource::validParams()
{
  InputParameters params = Material::validParams();
  params.addParam<Real>("power", 1, "Input power of the heat source");
  params.addParam<FunctionName>("function_power", "power");
  params.addParam<Real>("efficiency", 1, "process efficiency");
  params.addParam<FunctionName>("function_efficiency", "process efficiency");
  params.addParam<Real>("rx", "transverse ellipsoid axe");
  params.addParam<Real>("ry", "depth ellipsoid axe");
  params.addParam<Real>("rz", "longitudinal ellipsoid axe");
  params.addParam<FunctionName>("heat_source_rx", "effective transverse ellipsoid radius");
  params.addParam<FunctionName>("heat_source_ry", "effective longitudinal ellipsoid radius");
  params.addParam<FunctionName>("heat_source_rz", "effective depth ellipsoid radius");
  params.addParam<Real>(
      "factor", 1, "scaling factor that is multiplied to the heat source to adjust the intensity");
  params.addParam<FunctionName>(
      "function_x", "0", "The x component of the center of the heating spot as a function of time");
  params.addParam<FunctionName>(
      "function_y", "0", "The y component of the center of the heating spot as a function of time");
  params.addParam<FunctionName>(
      "function_z", "0", "The z component of the center of the heating spot as a function of time");
  params.addParam<std::string>("path",
                               "The name of the spatio-temporal path object that describes "
                               "the moving path of the heat source.");
  params.addParam<FunctionName>("function_weave_amp_x",
                                "The x component of the weave amplitude as a function of time");
  params.addParam<FunctionName>("function_weave_amp_y",
                                "The y component of the weave amplitude as a function of time");
  params.addParam<FunctionName>("function_weave_amp_z",
                                "The z component of the weave amplitude as a function of time");

  params.addParam<FunctionName>("function_torch_speed",
                                "The speed of the torch as a function of time");

  params.addParam<Real>(
      "wavelength",
      1,
      "The wavelength of the weave pattern. If set to 0, no weave pattern is applied.");

  params.addParam<PostprocessorName>("va_postprocess", "The postprocessor on the source");

  params.addClassDescription("Double ellipsoid volumetric source heat with function path.");

  return params;
}

FunctionPathEllipsoidHeatSource::FunctionPathEllipsoidHeatSource(const InputParameters & parameters)
  : Material(parameters),
    _function_P(isParamSetByUser("function_power") ? &getFunction("function_power") : nullptr),
    _P(getParam<Real>("power")),
    _eta(getParam<Real>("efficiency")),
    _function_efficiency(
        isParamSetByUser("function_efficiency") ? &getFunction("function_efficiency") : nullptr),
    _function_rx(isParamSetByUser("heat_source_rx") ? &getFunction("heat_source_rx") : nullptr),
    _function_ry(isParamSetByUser("heat_source_ry") ? &getFunction("heat_source_ry") : nullptr),
    _function_rz(isParamSetByUser("heat_source_rz") ? &getFunction("heat_source_rz") : nullptr),
    _rx(isParamSetByUser("rx") ? getParam<Real>("rx") : 0.0),
    _ry(isParamSetByUser("ry") ? getParam<Real>("ry") : 0.0),
    _rz(isParamSetByUser("rz") ? getParam<Real>("rz") : 0.0),
    _f(getParam<Real>("factor")),
    _function_x(getFunction("function_x")),
    _function_y(getFunction("function_y")),
    _function_z(getFunction("function_z")),
    _function_weave_amp_x(
        isParamSetByUser("function_weave_amp_x") ? &getFunction("function_weave_amp_x") : nullptr),
    _function_weave_amp_y(
        isParamSetByUser("function_weave_amp_y") ? &getFunction("function_weave_amp_y") : nullptr),
    _function_weave_amp_z(
        isParamSetByUser("function_weave_amp_z") ? &getFunction("function_weave_amp_z") : nullptr),
    _function_torch_speed(
        isParamSetByUser("function_torch_speed") ? &getFunction("function_torch_speed") : nullptr),
    _wavelength(getParam<Real>("wavelength")),
    _volumetric_heat(declareADProperty<Real>("volumetric_heat")),
    _path(isParamSetByUser("path") ? &getUserObjectByName<SpatioTemporalPath>("path") : nullptr),
    _va_integral(isParamSetByUser("va_postprocess") ? &getPostprocessorValue("va_postprocess")
                                                    : nullptr)
{

  if (!_function_rx && _rx == 0.0)
    mooseError(
        "Either 'heat_source_rx' function or 'rx' parameter must be set to a non-zero value.");
  if (!_function_ry && _ry == 0.0)
    mooseError(
        "Either 'heat_source_ry' function or 'ry' parameter must be set to a non-zero value.");
  if (!_function_rz && _rz == 0.0)
    mooseError(
        "Either 'heat_source_rz' function or 'rz' parameter must be set to a non-zero value.");
}

void
FunctionPathEllipsoidHeatSource::computeQpProperties()
{
  const Real & x = _q_point[_qp](0);
  const Real & y = _q_point[_qp](1);
  const Real & z = _q_point[_qp](2);

  Real x_t, y_t, z_t;
  if (!_path)
  {
    // center of the heat source
    x_t = _function_x.value(_t);
    y_t = _function_y.value(_t);
    z_t = _function_z.value(_t);
  }
  else
  {
    Point p_t = _path->position(_t);
    // center of the heat source
    x_t = p_t(0);
    y_t = p_t(1);
    z_t = p_t(2);
  }

  Real freq = 0.0;
  if (_function_weave_amp_x || _function_weave_amp_y || _function_weave_amp_z)
    if (!_function_torch_speed)
      mooseError("The weave amplitude functions are set, but the torch speed function is not set. "
                 "Please set the 'function_torch_speed' parameter to a valid function name.");
    else if (_wavelength == 0)
      mooseError("The wavelength is set to 0, but the weave amplitude functions are set. "
                 "Please set the 'wavelength' parameter to a non-zero value or remove the weave "
                 "amplitude functions.");
    else
      freq = _function_torch_speed->value(_t) / _wavelength;

  if (_function_weave_amp_x)
    x_t += _function_weave_amp_x->value(_t) * std::sin(2.0 * libMesh::pi * freq * _t);

  if (_function_weave_amp_y)
    y_t += _function_weave_amp_y->value(_t) * std::sin(2.0 * libMesh::pi * freq * _t);

  if (_function_weave_amp_z)
    z_t += _function_weave_amp_z->value(_t) * std::sin(2.0 * libMesh::pi * freq * _t);

  Real eta = (_function_efficiency) ? _function_efficiency->value(_t) : _eta;

  Real rx = (_function_rx) ? _function_rx->value(_t) : _rx;
  Real ry = (_function_ry) ? _function_ry->value(_t) : _ry;
  Real rz = (_function_rz) ? _function_rz->value(_t) : _rz;

  Real P = (_function_P) ? _function_P->value(_t) : _P;

  _volumetric_heat[_qp] = _va_integral
                              ? P * eta * _f *
                                    std::exp(-(std::pow(x - x_t, 2.0) / std::pow(rx, 2.0) +
                                               std::pow(y - y_t, 2.0) / std::pow(ry, 2.0) +
                                               std::pow(z - z_t, 2.0) / std::pow(rz, 2.0))) /
                                    *_va_integral
                              : 6.0 * std::sqrt(3.0) * P * eta * _f /
                                    (rx * ry * rz * std::pow(libMesh::pi, 1.5)) *
                                    std::exp(-(3.0 * std::pow(x - x_t, 2.0) / std::pow(rx, 2.0) +
                                               3.0 * std::pow(y - y_t, 2.0) / std::pow(ry, 2.0) +
                                               3.0 * std::pow(z - z_t, 2.0) / std::pow(rz, 2.0)));
}
