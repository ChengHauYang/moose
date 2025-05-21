#pragma once

#include "ElementSubdomainModifier.h"
#include "MooseEnum.h"
#include "Function.h"

#include "libmesh/quadrature_gauss.h"
#include "libmesh/fe.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_base.h"
#include "libmesh/elem.h"
#include "libmesh/point.h"
#include "libmesh/vtk_io.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/system.h"
#include "libmesh/dof_map.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include "WaterTightGeoReader.h"

class InterceptedElementModifier : public ElementSubdomainModifier /*, public UserObjectInterface*/
{
public:
  static InputParameters validParams();
  InterceptedElementModifier(const InputParameters & parameters);

protected:
  virtual SubdomainID computeSubdomainID() override;
  virtual void initialSetup() override;

  /// Store the parsed function
  const Function * _parsed_function;

private:
  /// IDs for subdomain classification
  SubdomainID _subdomain_id_inside;
  SubdomainID _subdomain_id_outside;

  /// Threshold value for classification
  Real _threshold;

  /// Lambda parameter for false intersection classification
  Real _lambda;

  /// Outer boundary handling flag
  bool _outer_boundary;

  Point _shift_geometry;

  int _qrule_order;

  const WaterTightGeoReader * _water_tight_geo_reader;

private:
  std::shared_ptr<KDTree> _external_kd_tree;
  std::shared_ptr<Mesh> _boundary_mesh;

  RayTracerBase * _ray_tracer = nullptr;

  InOutTest::DistanceType _in_out_test = InOutTest::DistanceType::NONE;

  void parse_water_tight_geom();

  inline libMesh::Order intToOrder(int value)
  {
    switch (value)
    {
      case 0:
        return libMesh::CONSTANT;
      case 1:
        return libMesh::FIRST;
      case 2:
        return libMesh::SECOND;
      case 3:
        return libMesh::THIRD;
      case 4:
        return libMesh::FOURTH;
      case 5:
        return libMesh::FIFTH;
      case 6:
        return libMesh::SIXTH;
      case 7:
        return libMesh::SEVENTH;
      case 8:
        return libMesh::EIGHTH;
      case 9:
        return libMesh::NINTH;
      case 10:
        return libMesh::TENTH;
      default:
        throw std::invalid_argument("Unsupported Order value: " + std::to_string(value));
    }
  }
};
