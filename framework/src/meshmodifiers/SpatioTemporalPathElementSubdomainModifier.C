#include "SpatioTemporalPathElementSubdomainModifier.h"

registerMooseObject("MooseApp", SpatioTemporalPathElementSubdomainModifier);

InputParameters
SpatioTemporalPathElementSubdomainModifier::validParams()
{
  InputParameters params = ElementSubdomainModifier::validParams();
  params.addClassDescription("Modify subdomain of elements when the element is within the "
                             "neighborhood of the path's current position.");
  params.addRequiredParam<SubdomainName>(
      "target_subdomain", "The subdomain name/ID to set when the path goes through an element");
  params.addRequiredParam<std::string>("path", "The name of the spatio-temporal path object.");
  params.addParam<Real>("radius",
                        0.0,
                        "The element subdomain is changed to the target subdomain if its "
                        "centroid is within the radius of the current path front.");

  params.addParam<FunctionName>(
      "function_radius",
      "The function that defines the radius of the path at the current time. "
      "If not set, the radius is constant and defined by the 'radius' parameter.");

  /// add parameter to swich between _within_elem test or _centroid test
  params.addParam<bool>("within_elem_test",
                        false,
                        "Switch between using the within element test or the centroid test.");
  return params;
}

SpatioTemporalPathElementSubdomainModifier::SpatioTemporalPathElementSubdomainModifier(
    const InputParameters & params)
  : ElementSubdomainModifier(params),
    SpatioTemporalPathInterface(this),
    _path(getSpatioTemporalPath("path")),
    _subdomain_id(_mesh.getSubdomainID(getParam<SubdomainName>("target_subdomain"))),
    _r(getParam<Real>("radius")),
    _function_radius(isParamSetByUser("function_radius") ? &getFunction("function_radius")
                                                         : nullptr),
    _within_elem(getParam<bool>("within_elem_test"))
{
  if (_r == 0.0 && !_function_radius)
    mooseError("Either 'radius' or 'function_radius' must be set to a non-zero value.");
}

SubdomainID
SpatioTemporalPathElementSubdomainModifier::computeSubdomainID()
{
  Real r = _function_radius ? _function_radius->value(_t) : _r;

  // std::cout << "SpatioTemporalPathElementSubdomainModifier::computeSubdomainID: "
  //           << "t = " << _t << ", r^2 = " << r * r << ", position = " << _path.position()
  //           << ", distance = " << (_current_elem->centroid() - _path.position()).norm_sq()
  //           << std::endl;
  // if ((_current_elem->centroid() - _path.position()).norm_sq() < r * r)
  //   std::cout << "Element centroid is within the radius." << std::endl;
  // else
  //   std::cout << "Element centroid is NOT within the radius." << std::endl;

  if (_within_elem)
  {
    if (_current_elem->contains_point(_path.position()))
    {
      return _subdomain_id;
    }
  }
  else
  {
    if ((_current_elem->centroid() - _path.position()).norm_sq() < r * r)
      return _subdomain_id;
  }

  return _current_elem->subdomain_id();
}
