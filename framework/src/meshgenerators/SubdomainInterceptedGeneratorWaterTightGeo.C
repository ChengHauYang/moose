//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SubdomainInterceptedGeneratorWaterTightGeo.h"
#include "Conversion.h"
#include "CastUniquePointer.h"
#include "MooseUtils.h"
#include "MooseMeshUtils.h"

#include "libmesh/elem.h"
#include "libmesh/quadrature.h"
#include "libmesh/quadrature_gauss.h"
#include "SetupQuadratureAction.h"

using namespace libMesh;

registerMooseObject("MooseApp", SubdomainInterceptedGeneratorWaterTightGeo);

InputParameters
SubdomainInterceptedGeneratorWaterTightGeo::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");

  params.addRequiredParam<SubdomainID>("subdomain_id_inside", "ID for inside elements.");
  params.addRequiredParam<SubdomainID>("subdomain_id_outside", "ID for outside elements.");
  params.addRequiredParam<SubdomainID>("subdomain_id_intercepted", "ID for intersected elements.");
  params.addRequiredParam<SubdomainID>("subdomain_id_false_intercected",
                                       "ID for false intersected elements.");
  params.addRequiredParam<SubdomainID>(
      "subdomain_id_neighbor_intercepted",
      "ID for neighbor of the intercepted elements. Only consider outside part");
  params.addParam<SubdomainID>("subdomain_id_original_intercepted",
                               6,
                               "If we use change_remain_intercepted_only = true, we will first "
                               "mark the intercepted element as oringinal intercepted element");

  params.addRequiredParam<Real>("threshold", "Threshold for inside/outside classification.");
  params.addRequiredParam<Real>("lambda", "Lambda for false intersection classification.");
  params.addRequiredParam<bool>("outer_boundary", "Flag for outer boundary handling.");
  params.addRequiredParam<bool>(
      "mark_neighbor_of_intercepted",
      "Whether we need to mark the element is the element near by the intercepted element.");
  params.addParam<bool>("multi_geo", true, "Do you have multiple geometries to do in-out test?");

  params.addParam<bool>("random_mark_some_intercepted_as_original_intercepted",
                        false,
                        "randomly mark some intercepted element as original intercepted element. "
                        "This is for the welding case.");

  params.addRequiredParam<bool>("outer_boundary", "Flag for outer boundary handling.");

  params.addRequiredParam<std::string>("water_tight_geo_path", "2D gmsh line path.");

  params.addParam<Real>("x_shift", 0.0, "shift geom in x-dir.");
  params.addParam<Real>("y_shift", 0.0, "shift geom in y-dir.");
  params.addParam<Real>("z_shift", 0.0, "shift geom in z-dir.");

  params.addParam<int>("qrule_order",
                       6 /*16 gauss points for quad element*/,
                       "Quadrature order for generating the Gauss points to do the IN-OUT test to "
                       "test whether the intercepted element belong to false intercepted element.");

  params.addParam<bool>(
      "simple_classification",
      true,
      "If true, elements are classified only as IN or OUT. "
      "If false, elements are classified as IN, OUT, false intercepted, or true intercepted.");

  return params;
}

SubdomainInterceptedGeneratorWaterTightGeo::SubdomainInterceptedGeneratorWaterTightGeo(
    const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getMesh("input")),
    _subdomain_id_inside(getParam<SubdomainID>("subdomain_id_inside")),
    _subdomain_id_outside(getParam<SubdomainID>("subdomain_id_outside")),
    _subdomain_id_intercepted(getParam<SubdomainID>("subdomain_id_intercepted")),
    _subdomain_id_false_intercected(getParam<SubdomainID>("subdomain_id_false_intercected")),
    _subdomain_id_neighbor_intercepted(getParam<SubdomainID>("subdomain_id_neighbor_intercepted")),
    _subdomain_id_original_intercepted(getParam<SubdomainID>("subdomain_id_original_intercepted")),
    _threshold(getParam<Real>("threshold")),
    _lambda(getParam<Real>("lambda")),
    _outer_boundary(getParam<bool>("outer_boundary")),
    _mark_neighbor_of_intercepted(getParam<bool>("mark_neighbor_of_intercepted")),
    _random_mark_some_intercepted_as_original_intercepted(
        getParam<bool>("random_mark_some_intercepted_as_original_intercepted")),
    _multi_geo(getParam<bool>("multi_geo")),
    _water_tight_geo_path(getParam<std::string>("water_tight_geo_path")),
    _shift_geometry(
        Point(getParam<Real>("x_shift"), getParam<Real>("y_shift"), getParam<Real>("z_shift"))),
    _qrule_order(getParam<int>("qrule_order")),
    _simple_classification(getParam<bool>("simple_classification")),
    _line2d_data(""),
    _stl_data("")
{
}

std::unique_ptr<libMesh::MeshBase>
SubdomainInterceptedGeneratorWaterTightGeo::generate()
{
  std::unique_ptr<libMesh::MeshBase> mesh = std::move(_input);
  std::unordered_set<const libMesh::Node *> intersected_nodes;
  std::unordered_map<const libMesh::Elem *, SubdomainID> classification;

  parse_water_tight_geom();

  RayTracerBase * ray_tracer = nullptr;

  if (!_line2d_data.lines.empty())
  {
    ray_tracer = new RayTracer2D(&_line2d_data, _line2d_data.lines.size());
    if (mesh->mesh_dimension() == 3)
    {
      mooseWarning(
          "2D geometry is used for 3D mesh. The 2D geometry will be projected to the 3D mesh.");
    }
  }
  else if (!_stl_data.triangles.empty())
  {
    // placeholder for STL data
    ray_tracer = new RayTracer3D(&_stl_data, _stl_data.triangles.size());
    if (mesh->mesh_dimension() == 2)
    {
      mooseWarning(
          "3D geometry is used for 2D mesh. The 3D geometry will be projected to the 2D mesh.");
    }
  }

  // First pass: Classify elements and store classification in a map
  for (const auto & elem : mesh->active_local_element_ptr_range())
  {
    Real min_criterion = std::numeric_limits<Real>::max();
    Real max_criterion = std::numeric_limits<Real>::lowest();
    std::vector<Point> node_locations(elem->n_nodes());

    /// keep original outside/intercepted as outside/intercepted
    if (_multi_geo and (elem->subdomain_id() == _subdomain_id_outside ||
                        elem->subdomain_id() == _subdomain_id_intercepted))
    {
      continue;
    }

    int nodal_pt_number = elem->n_nodes();

    int active_pt_number = 0;

    for (unsigned int node = 0; node < elem->n_nodes(); ++node)
    {
      node_locations[node] = elem->point(node);
      Point node_loc = elem->point(node);

      if (ray_tracer->ifInside(node_loc))
      {
        active_pt_number++;
      }
    }

    // std::cout << "active_pt_number: " << active_pt_number << std::endl;
    // std::cout << "nodal_pt_number: " << nodal_pt_number << std::endl;
    if (active_pt_number == nodal_pt_number)
    {
      elem->subdomain_id() = _outer_boundary ? _subdomain_id_inside : _subdomain_id_outside;
      continue;
    }
    else if (active_pt_number == 0)
    {
      elem->subdomain_id() = _outer_boundary ? _subdomain_id_outside : _subdomain_id_inside;
      continue;
    }

    if (_lambda == 0)
    {
      elem->subdomain_id() =
          _simple_classification ? _subdomain_id_outside : _subdomain_id_false_intercected;
      // mark all intercepted element as false intercepted if lambda = 0
      continue;
    }

    if (_lambda == 1)
    {
      elem->subdomain_id() =
          _simple_classification ? _subdomain_id_inside : _subdomain_id_intercepted;
      // mark all intercepted element as true intercepted if lambda = 1
      continue;
    }

    // Convert int to libMesh::Order
    libMesh::Order order = intToOrder(_qrule_order);

    const unsigned int dim = elem->dim();

    libMesh::FEType fe_type(elem->default_order(), LAGRANGE);

    std::unique_ptr<libMesh::FEBase> fe(libMesh::FEBase::build(dim, fe_type));

    libMesh::QGauss qrule(dim, order);
    fe->attach_quadrature_rule(&qrule);
    const std::vector<Point> & normals = fe->get_normals();
    fe->get_xyz(); // this is very important, otherwise the quadrature points are not
                   // initialized
    fe->get_JxW();
    fe->reinit(elem);

    const std::vector<Real> & JxW = fe->get_JxW();
    const std::vector<Point> & q_points = fe->get_xyz();

#ifndef NDEBUG
    /// Debug
    fe->print_JxW(std::cout);
    fe->print_xyz(std::cout);
#endif

    double activePtArea = 0;
    double totalPtArea = 0;
    int gp_idx = 0;
    for (const Point & p : q_points)
    {
      if ((ray_tracer->ifInside(p) && _outer_boundary) ||
          (!ray_tracer->ifInside(p) && !_outer_boundary))
      {
        activePtArea += JxW[gp_idx];
      }
      totalPtArea += JxW[gp_idx];
      gp_idx++;
    }

    Real ratio_active = activePtArea / totalPtArea; // placeholder for the number of nodes

    if ((1 - ratio_active) > _lambda)
    {
      elem->subdomain_id() =
          _simple_classification ? _subdomain_id_outside : _subdomain_id_false_intercected;
    }
    else
    {
      if (_random_mark_some_intercepted_as_original_intercepted)
      {
        elem->subdomain_id() = _simple_classification
                                   ? _subdomain_id_inside
                                   : ((std::rand() % 2 == 0) ? _subdomain_id_intercepted
                                                             : _subdomain_id_original_intercepted);
      }
      else
      {
        elem->subdomain_id() =
            _simple_classification ? _subdomain_id_inside : _subdomain_id_intercepted;
      }

      // Store nodes of intersected elements
      for (unsigned int node = 0; node < elem->n_nodes(); ++node)
        intersected_nodes.insert(elem->node_ptr(node));
    }
  }

  if (_mark_neighbor_of_intercepted)
  {
    // std::cout << "_mark_neighbor_of_intercepted\n";
    // Second pass: Mark neighboring elements
    for (const auto & elem : mesh->active_local_element_ptr_range())
    {
      if (elem->subdomain_id() == _subdomain_id_outside)
      {
        for (unsigned int node = 0; node < elem->n_nodes(); ++node)
        {
          if (intersected_nodes.count(elem->node_ptr(node)))
          {
            if (!_simple_classification)
              elem->subdomain_id() = _subdomain_id_neighbor_intercepted;
            break;
          }
        }
      }
    }
  }

  mesh->set_isnt_prepared();

  delete ray_tracer;
  return mesh;
}

void
SubdomainInterceptedGeneratorWaterTightGeo::parse_water_tight_geom()
{
  _boundary_mesh = std::make_unique<Mesh>(_communicator);

  _boundary_mesh->read(_water_tight_geo_path);

  std::cout << "after reading\n";

  int dim_original_mesh = _boundary_mesh->mesh_dimension() + 1;

  switch (dim_original_mesh)
  {
    case 2:
    {
      _line2d_data.lines.reserve(_boundary_mesh->n_elem());
      for (const auto & b_elem : _boundary_mesh->element_ptr_range())
      {
        // Shifted points
        Point p1 = b_elem->node_ref(0) + _shift_geometry;
        Point p2 = b_elem->node_ref(1) + _shift_geometry;

        // Compute direction vector
        Point dir = p2 - p1;

        // Compute 2D normal vector (counterclockwise)
        Point normal(-dir(1), dir(0), 0.0);

        // Normalize the normal vector
        Real length = std::sqrt(normal(0) * normal(0) + normal(1) * normal(1));
        if (length > 1e-12)
          normal /= length;

        // Create Line with normal
        Line line(p1, p2, normal);
        _line2d_data.lines.push_back(line);
      }
      break;
    }
    case 3:
    {
      // std::cout << "_boundary_mesh->n_elem() = " << _boundary_mesh->n_elem() << std::endl;
      _stl_data.triangles.reserve(_boundary_mesh->n_elem());

      for (const auto & b_elem : _boundary_mesh->element_ptr_range())
      {

        Point v1 = b_elem->node_ref(0) + _shift_geometry;
        Point v2 = b_elem->node_ref(1) + _shift_geometry;
        Point v3 = b_elem->node_ref(2) + _shift_geometry;
        Point normal_calculated =
            (v2 - v1).cross(v3 - v1); // Cross product to calculate the normal vector
        // Normalize the normal vector
        Real length = std::sqrt(normal_calculated(0) * normal_calculated(0) +
                                normal_calculated(1) * normal_calculated(1) +
                                normal_calculated(2) * normal_calculated(2));
        if (length > 1e-12)
          normal_calculated /= length;

        _stl_data.triangles.emplace_back(Triangle(v1, v2, v3, normal_calculated));
      }

      break;
    }
  }
}

float
SubdomainInterceptedGeneratorWaterTightGeo::parse_float(std::ifstream & s)
{
  char f_buf[sizeof(float)];
  s.read(f_buf, 4);
  auto * fptr = reinterpret_cast<float *>(f_buf);
  return *fptr;
}

Point
SubdomainInterceptedGeneratorWaterTightGeo::parse_point(std::ifstream & s)
{
  float x = parse_float(s);
  float y = parse_float(s);
  float z = parse_float(s);
  return Point(x, y, z);
}
