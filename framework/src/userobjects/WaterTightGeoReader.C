// WaterTightGeoReader.h/.C

#include "WaterTightGeoReader.h"
#include "MooseMesh.h"
#include "InputParameters.h"
#include "libmesh/mesh_generation.h"
#include "libmesh/point.h"
#include "libmesh/elem.h"
#include "libmesh/mesh.h"
#include <cmath>
#include <iomanip>

// Register object
registerMooseObject("MooseApp", WaterTightGeoReader);

InputParameters
WaterTightGeoReader::validParams()
{
  InputParameters params = GeneralUserObject::validParams();
  params.addClassDescription(
      "The user object that reads a water-tight geometry file and parses it.");

  params.addRequiredParam<std::string>("mesh_file", "Path to the watertight Gmsh file");
  params.addParam<Real>("x_shift", 0.0, "shift geom in x-dir.");
  params.addParam<Real>("y_shift", 0.0, "shift geom in y-dir.");
  params.addParam<Real>("z_shift", 0.0, "shift geom in z-dir.");

  return params;
}

WaterTightGeoReader::WaterTightGeoReader(const InputParameters & parameters)
  : GeneralUserObject(parameters),
    _mesh_file(getParam<std::string>("mesh_file")),
    _shift_geometry(
        Point(getParam<Real>("x_shift"), getParam<Real>("y_shift"), getParam<Real>("z_shift"))),
    _elem_id_map(std::make_shared<std::vector<std::size_t>>())
{
}

void
WaterTightGeoReader::initialSetup()
{
  // Assume the mesh file is a GMSH file
  _boundary_mesh = std::make_shared<Mesh>(_communicator);
  _boundary_mesh->read(_mesh_file);

  // Shift the mesh
  for (auto & node : _boundary_mesh->node_ptr_range())
    *node += _shift_geometry;

  _dim_original_mesh = _boundary_mesh->mesh_dimension() + 1;
  buildKDtree();
}

void
WaterTightGeoReader::buildKDtree()
{
  const std::size_t num_elems = _boundary_mesh->n_elem();

  _mid_points.resize(num_elems);
  _elem_id_map->resize(num_elems);

  int i = 0;

  for (auto elem_it = _boundary_mesh->active_elements_begin();
       elem_it != _boundary_mesh->active_elements_end();
       elem_it++) /*loop over active element!*/
  {
    _mid_points[i] = (*elem_it)->vertex_average();
    _elem_id_map->at(i) = (*elem_it)->id();
    ++i;
  }

  _kd_tree = std::make_shared<KDTree>(_mid_points, 10);
  MPI_Barrier(MPI_COMM_WORLD);
}
