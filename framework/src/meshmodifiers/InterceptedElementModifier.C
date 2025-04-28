//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "InterceptedElementModifier.h"

registerMooseObject("MooseApp", InterceptedElementModifier);

InputParameters
InterceptedElementModifier::validParams()
{
  InputParameters params = ElementSubdomainModifier::validParams();

  params.addClassDescription("Marks elements as inside, outside, or intersected based on a given "
                             "distance function or geometry.");

  params.addParam<FunctionName>("function", "Function to evaluate");

  params.addRequiredParam<SubdomainID>("subdomain_id_inside", "ID for inside elements.");
  params.addRequiredParam<SubdomainID>("subdomain_id_outside", "ID for outside elements.");

  params.addParam<Real>("threshold", 0, "Threshold for inside/outside classification.");
  params.addRequiredParam<Real>("lambda", "Lambda for false intersection classification.");
  params.addRequiredParam<bool>("outer_boundary", "Flag for outer boundary handling.");
  params.addParam<bool>(
      "mark_neighbor_of_intercepted",
      false,
      "Whether we need to mark the element is the element near by the intercepted element.");

  params.addParam<Real>("x_shift", 0.0, "shift geom in x-dir.");
  params.addParam<Real>("y_shift", 0.0, "shift geom in y-dir.");
  params.addParam<Real>("z_shift", 0.0, "shift geom in z-dir.");

  params.addParam<int>("qrule_order",
                       6 /*16 gauss points for quad element*/,
                       "Quadrature order for generating the Gauss points to do the IN-OUT test to "
                       "test whether the intercepted element belong to false intercepted element.");

  params.addParam<SubdomainID>("subdomain_id_original_intercepted",
                               6,
                               "If we use change_remain_intercepted_only = true, we will first "
                               "mark the intercepted element as oringinal intercepted element");
  params.addRequiredParam<UserObjectName>("water_tight_geo_reader",
                                          "the name of the water tight geo reader user object");

  return params;
}

InterceptedElementModifier::InterceptedElementModifier(const InputParameters & parameters)
  : ElementSubdomainModifier(parameters),
    _parsed_function(isParamSetByUser("function")
                         ? &getFunctionByName(parameters.get<FunctionName>("function"))
                         : nullptr),
    _subdomain_id_inside(getParam<SubdomainID>("subdomain_id_inside")),
    _subdomain_id_outside(getParam<SubdomainID>("subdomain_id_outside")),
    _threshold(getParam<Real>("threshold")),
    _lambda(getParam<Real>("lambda")),
    _outer_boundary(getParam<bool>("outer_boundary")),
    _shift_geometry(
        Point(getParam<Real>("x_shift"), getParam<Real>("y_shift"), getParam<Real>("z_shift"))),
    _qrule_order(getParam<int>("qrule_order")),
    _water_tight_geo_reader(isParamSetByUser("water_tight_geo_reader")
                                ? &getUserObject<WaterTightGeoReader>("water_tight_geo_reader")
                                : nullptr)
{
}

/// @brief Initial setup for the InterceptedElementModifier class to read in the Gmsh file
/// NOTE: this function should be overrided
void
InterceptedElementModifier::initialSetup()
{
  if (isParamSetByUser("water_tight_geo_reader"))
  {
    std::cout << "InterceptedElementModifier: water_tight_geo_reader is set!" << std::endl;
  }
  else
  {
    std::cout << "InterceptedElementModifier: water_tight_geo_reader is not set!" << std::endl;
  }

  if (_water_tight_geo_reader)
    _in_out_test = InOutTest::DistanceType::GEOMETRY;
  else if (_parsed_function)
    _in_out_test = InOutTest::DistanceType::SIGN_DISTANCE;
  else
    mooseError(
        "InterceptedElementModifier: _water_tight_geo_reader and _parsed_function are null!");

  if (_in_out_test == InOutTest::DistanceType::GEOMETRY)
  {
    if (_water_tight_geo_reader->getDimOriginalMesh() == 2)
    {
      _boundary_mesh = _water_tight_geo_reader->getBoundaryMesh();
      _ray_tracer = new RayTracer2D(_boundary_mesh.get());
    }
    else if (_water_tight_geo_reader->getDimOriginalMesh() == 3)
    {
      _boundary_mesh = _water_tight_geo_reader->getBoundaryMesh();
      _ray_tracer = new RayTracer3D(_boundary_mesh.get());
    }
    else
      mooseError("InterceptedElementModifier: _water_tight_geo_reader is not 2D or 3D!");
  }
}

SubdomainID
InterceptedElementModifier::computeSubdomainID()
{
  const Elem * elem = this->_current_elem;
  if (!elem)
    mooseError("InterceptedElementModifier: _current_elem is null!");

  auto check_lambda_flags = [&](Real ratio_active) -> SubdomainID
  {
    if (_lambda == 0)
      return _subdomain_id_outside;
    if (_lambda == 1)
      return _subdomain_id_inside;
    return (1 - ratio_active > _lambda) ? _subdomain_id_outside : _subdomain_id_inside;
  };

  auto initFEBase = [&](const Elem * elem)
  {
    libMesh::Order order = intToOrder(_qrule_order);
    libMesh::FEType fe_type(elem->default_order(), LAGRANGE);
    std::unique_ptr<libMesh::FEBase> fe(libMesh::FEBase::build(elem->dim(), fe_type));
    libMesh::QGauss qrule(elem->dim(), order);
    fe->attach_quadrature_rule(&qrule);
    fe->reinit(elem);
    return fe;
  };

  auto computeActiveAreaRatio =
      [&](const Elem * elem, const std::function<bool(const Point &)> & is_active)
  {
    auto fe = initFEBase(elem);
    const auto & JxW = fe->get_JxW();
    const auto & q_points = fe->get_xyz();
    double active_area = 0, total_area = 0;

    for (unsigned int i = 0; i < q_points.size(); ++i)
    {
      if (is_active(q_points[i]))
        active_area += JxW[i];
      total_area += JxW[i];
    }
    return active_area / total_area;
  };

  if (_in_out_test == InOutTest::DistanceType::SIGN_DISTANCE)
  {
    Real min_val = std::numeric_limits<Real>::max();
    Real max_val = std::numeric_limits<Real>::lowest();

    for (unsigned int node = 0; node < elem->n_nodes(); ++node)
    {
      Real val = _parsed_function->value(_t, elem->point(node));
      min_val = std::min(min_val, val);
      max_val = std::max(max_val, val);
    }

    if (max_val < _threshold)
      return _outer_boundary ? _subdomain_id_inside : _subdomain_id_outside;
    else if (min_val > _threshold)
      return _outer_boundary ? _subdomain_id_outside : _subdomain_id_inside;

    auto is_active = [&](const Point & p)
    {
      Real val = _parsed_function->value(_t, p);
      return (_outer_boundary && val < _threshold) || (!_outer_boundary && val > _threshold);
    };

    Real ratio_active = computeActiveAreaRatio(elem, is_active);
    return check_lambda_flags(ratio_active);
  }
  else if (_in_out_test == InOutTest::DistanceType::GEOMETRY)
  {
    int active_nodes = 0;
    for (unsigned int node = 0; node < elem->n_nodes(); ++node)
    {
      if (_ray_tracer->ifInside(elem->point(node)))
        ++active_nodes;
    }

    if (active_nodes == elem->n_nodes())
      return _outer_boundary ? _subdomain_id_inside : _subdomain_id_outside;
    else if (active_nodes == 0)
      return _outer_boundary ? _subdomain_id_outside : _subdomain_id_inside;

    auto is_active = [&](const Point & p)
    {
      return (_outer_boundary && _ray_tracer->ifInside(p)) ||
             (!_outer_boundary && !_ray_tracer->ifInside(p));
    };

    Real ratio_active = computeActiveAreaRatio(elem, is_active);
    return check_lambda_flags(ratio_active);
  }
  else
  {
    mooseError("InterceptedElementModifier: Unknown InOutTest type!");
  }

  return -1; // fallback (shouldn't reach)
}
