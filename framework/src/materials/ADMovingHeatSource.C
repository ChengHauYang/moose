#include "ADMovingHeatSource.h"

InputParameters
ADMovingHeatSource::validParams()
{
  auto params = Material::validParams();
  params.addRequiredParam<std::string>("path",
                                       "The name of the spatio-temporal path object that describes "
                                       "the moving path of the heat source.");
  params.addParam<MaterialPropertyName>("heat_source_tangential_distance",
                                        "heat_source_tangential_distance",
                                        "The name of the material property used to store the "
                                        "tangential distance from the heat source.");
  params.addParam<MaterialPropertyName>("heat_source_normal_distance",
                                        "heat_source_normal_distance",
                                        "The name of the material property used to store the "
                                        "normal distance from the heat source.");
  return params;
}

ADMovingHeatSource::ADMovingHeatSource(const InputParameters & params)
  : Material(params),
    SpatioTemporalPathInterface(this),
    _path(getSpatioTemporalPath("path")),
    _tangential_distance(declareProperty<Real>("heat_source_tangential_distance")),
    _normal_distance(declareProperty<Real>("heat_source_normal_distance")),
    _volumetric_heat(declareADProperty<Real>("volumetric_heat"))
{
}

void
ADMovingHeatSource::computeQpProperties()
{

  const Point & q_point = _q_point[_qp];

  _tangential_distance[_qp] = _path.tangentialDistance(_q_point[_qp]);
  _normal_distance[_qp] = _path.normalDistance(_q_point[_qp]);
  _volumetric_heat[_qp] = computeHeatSource();

  // if (_volumetric_heat[_qp] > 100)
  // {
  //   std::cout << "point = ";
  //   q_point.print();
  //   std::cout << std::endl;
  //   std::cout << "tangential_distance = " << _tangential_distance[_qp] << std::endl;
  //   std::cout << "normal_distance = " << _normal_distance[_qp] << std::endl;
  //   std::cout << "volumetric_heat = " << _volumetric_heat[_qp] << std::endl;
  //   std::cout << "----------------------------------------" << std::endl;
  // }
}
