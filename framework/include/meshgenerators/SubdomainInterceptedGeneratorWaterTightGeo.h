//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MeshGenerator.h"
#include "MooseEnum.h"
#include "Function.h"
#include "MooseApp.h"
#include "MooseMesh.h"
#include "libmesh/bounding_box.h"
#include <cmath>
#include "libmesh/mesh.h"

namespace BinningType
{
enum Type
{
  NOBIN = -1,
  X_1D = 0,  // 2D + 3D
  Y_1D = 1,  // 2D + 3D
  Z_1D = 2,  // 3D
  XY_2D = 3, // 3D
  YZ_2D = 4, // 3D
  ZX_2D = 5, // 3D
};
}

namespace InsideType
{
/**
 * Type < 0   -> error
 * Type == 0  -> outside
 * Type > 0   -> inside or on surface
 */
enum Type
{
  UNKNOWN = -1,
  OUTSIDE = 0,
  INSIDE = 1,
  ON_SURFACE = 2,
};
}

namespace IntersectionType
{
enum Type
{
  INTERSECT = 0,
  PARALLEL = 1,
  ON_GEO_DIM_MINUS_1 = 2, // ON_NODE(2D), ON_EDGE(3D)
  ON_GEO_DIM = 3,         // ON_LINE(2D), ON_SURFACE(3D)
  NO_INTERSECTION = 4,
  ON_STARTPOINT = 5,
};
}

struct Circle
{
  Point center;
  double radius;
  Circle() : center{}, radius{} {};
  Circle(float xc, float yc, float radius) : center(Point{xc, yc, 0.0}), radius{radius} {}

  static Circle fromLine(const Point p1, const Point p2)
  {
    double xc = (p1(0) + p2(0)) / 2;
    double yc = (p1(1) + p2(1)) / 2;
    double distance_1 = std::sqrt(std::pow(p1(0) - xc, 2) + std::pow(p2(1) - yc, 2));
    double distance_2 = std::sqrt(std::pow(p1(0) - xc, 2) + std::pow(p2(1) - yc, 2));
    double radius = distance_1;
    if (radius < distance_2)
    {
      radius = distance_2;
    }
    double eps = 1e-2;
    return {static_cast<float>(xc), static_cast<float>(yc), static_cast<float>(radius * (1 + eps))};
  }
};

struct Sphere
{
  Point center;
  double radius;
  Sphere() : center{}, radius{} {};
  Sphere(float xc, float yc, float zc, float radius) : center(Point{xc, yc, zc}), radius{radius} {}

  static Sphere fromTriangle(const std::vector<Point> & v)
  {

    assert(v.size() == 3); // ensure it's a triangle

    Point cg = (v[0] + v[1] + v[2]) / 3;
    double xc = cg(0);
    double yc = cg(1);
    double zc = cg(2);

    // Lambda to compute distance from centroid
    auto distance = [&](const Point & p) -> double
    { return std::sqrt(std::pow(p(0) - xc, 2) + std::pow(p(1) - yc, 2) + std::pow(p(2) - zc, 2)); };

    // Compute the maximum distance to find the radius
    double radius = std::max({distance(v[0]), distance(v[1]), distance(v[2])});

    // Slightly inflate the radius
    double eps = 1e-2;
    radius *= (1.0 + eps);

    return Sphere{static_cast<float>(xc),
                  static_cast<float>(yc),
                  static_cast<float>(zc),
                  static_cast<float>(radius)};
  }
};

struct Line
{
  Point p1;
  Point p2;
  Point normal_vector;

  Line(const Point & pt1, const Point & pt2, const Point & normal)
    : p1(pt1), p2(pt2), normal_vector(normal)
  {
  }

  Circle boundingCircle() const { return Circle::fromLine(p1, p2); };
};

struct Line2DData
{
  std::string name;
  std::vector<Line> lines;

  void print()
  {
    std::cout << "Line2DData: " << name << std::endl;
    for (const auto & line : lines)
    {
      std::cout << "  " << line.p1 << " -> " << line.p2 << std::endl;
    }
  }

  explicit Line2DData(const std::string & namep) : name(namep) {}
};

struct Triangle
{
  std::vector<Point> v;
  Point p1, p2, p3;
  Point normal;

  Triangle(const Point & v1, const Point & v2, const Point & v3, const Point & normal_in = {})
    : v{v1, v2, v3}, p1(v1), p2(v2), p3(v3), normal(normal_in)
  {
  }

  void print() const
  {
    std::cout << "Triangle: " << p1 << " -> " << p2 << " -> " << p3 << std::endl;
  }

  Sphere boundingSphere() const { return Sphere::fromTriangle(this->v); };
};

struct STLData
{
  std::string name;

  //  std::string name;
  std::vector<Triangle> triangles;

  void validate()
  {
    for (const auto & triangle : triangles)
    {
      for (auto & vertex : triangle.v)
      {
        vertex.print();
        std::cout << "\n";
      }
    }
  }

  explicit STLData(const std::string & namep) : name(namep) {}
};

class RayTracerBase
{
public:
  virtual ~RayTracerBase() = default;
  virtual bool ifInside(const Point & p) = 0;

protected:
  int _dim = -1;
  BoundingBox _bounds;
  std::vector<Point> _ray_starts;
  BinningType::Type _binning_type;

  virtual BoundingBox calcBoundingBox() = 0;
  virtual void generateRay(int pick, Point & result) = 0;
  virtual void setBinning(const int target_per_bin = 10) = 0;

  bool ifOutsideBoundingBox(const Point ray_end /*the point we want to determine IN-OUT*/) const
  {
    mooseAssert(_dim > 0, "RayTracerBase::ifOutsideBoundingBox: _dim not set");
    const auto & min = _bounds.min();
    const auto & max = _bounds.max();

    return (_dim == 2) ? ((ray_end(0) < min(0)) || (ray_end(0) > max(0)) || (ray_end(1) < min(1)) ||
                          (ray_end(1) > max(1)))
                       : ((ray_end(0) < min(0)) || (ray_end(0) > max(0)) || (ray_end(1) < min(1)) ||
                          (ray_end(1) > max(1)) || (ray_end(2) < min(2)) || (ray_end(2) > max(2)));
  }

  bool OutsideRayBBOX(const Point & orig,
                      const Point & dir,
                      const Point & center,
                      const double radius) const
  {
    Point lb, ub;

    for (int i = 0; i < _dim; ++i)
    {
      lb(i) = std::min(orig(i), orig(i) + dir(i)) - radius;
      ub(i) = std::max(orig(i), orig(i) + dir(i)) + radius;
    }

    for (int i = 0; i < _dim; ++i)
    {
      if (center(i) < lb(i) || center(i) > ub(i))
        return true; // outside bbox
    }

    return false; // inside bbox
  }

  bool OutsideBoundingSphereOrCircle(const Point & orig,
                                     const Point & dir,
                                     const Point & center,
                                     const double radius) const
  {
    Point w = center - orig;
    double b = (w * dir) / (dir * dir);

    Point Pb = orig + b * dir;

    double distance_square = 0.0;
    for (int i = 0; i < _dim; ++i)
      distance_square += (Pb(i) - center(i)) * (Pb(i) - center(i));

    return (distance_square > radius * radius);
  }
};

struct RayTracer3D : public RayTracerBase
{
  RayTracer3D(const STLData * stl, const int num_triangles)
    : _stl(stl), _num_triangles(num_triangles)
  {
    _dim = 3;
    _binning_type = BinningType::Type::NOBIN;

    _ray_starts.resize(8);
    // Initialize bounding box
    _bounds = calcBoundingBox();

    for (int i = 0; i < 8; ++i)
    {
      generateRay(i, _ray_starts[i]);
    }

    setBinning(10);
  }

  bool ifInside(const Point & point) override
  {
    Point ray_start;
    unsigned int binno[2];
    generateRayStartAndBinIndex(point, ray_start, binno);

    InsideType::Type res = traceRayAgainstGeometry(ray_start, point, binno);

    /// falling back to original ray-tracing
    if (res == InsideType::UNKNOWN)
    {
      for (int i = 0; i < 8; i++)
      {
        res = traceRayAgainstGeometry(ray_start, point);
        if (res == InsideType::UNKNOWN)
          continue;
        return res;
      }
      return InsideType::UNKNOWN;
    }

    return res;
  }

private:
  const STLData * _stl;
  const int _num_triangles;

  std::vector<std::vector<std::pair<std::vector<double>, std::vector<unsigned int>>>> _bins_2d;

  BoundingBox calcBoundingBox() override
  {
    // Initialize bounding box with the first point
    Point min_pt = _stl->triangles[0].v[0]; // first triangle's first vertex
    Point max_pt = min_pt;

    for (int i = 0; i < _num_triangles; ++i)
    {
      // three vertices of the triangle
      const Point pts[3] = {
          _stl->triangles[i].v[0], _stl->triangles[i].v[1], _stl->triangles[i].v[2]};
      for (const auto & pt : pts)
      {
        for (unsigned int d = 0; d < 3; ++d)
        {
          min_pt(d) = std::min(min_pt(d), pt(d));
          max_pt(d) = std::max(max_pt(d), pt(d));
        }
      }
    }
    // Slightly expand the bounding box to avoid exact edge alignment issues
    const Real eps = 1e-6;
    for (unsigned int d = 0; d < 3; ++d)
    {
      min_pt(d) -= eps;
      max_pt(d) += eps;
    }

    return BoundingBox(min_pt, max_pt);
  }

  void generateRay(int pick, Point & result) override
  {
    const auto & min = _bounds.min();
    const auto & max = _bounds.max();

    const Real x_length = max(0) - min(0);
    const Real y_length = max(1) - min(1);
    const Real z_length = max(2) - min(2);

    // Generate small random perturbations > 1 to avoid edge alignment
    const Real random_perturb_x = 1.0 + static_cast<Real>(std::rand()) / RAND_MAX;
    const Real random_perturb_y = 1.0 + static_cast<Real>(std::rand()) / RAND_MAX;
    const Real random_perturb_z = 1.0 + static_cast<Real>(std::rand()) / RAND_MAX;

    switch (pick)
    {
      case 0: // (-x, -y, -z)
        result(0) = min(0) - x_length * random_perturb_x;
        result(1) = min(1) - y_length * random_perturb_y;
        result(2) = min(2) - z_length * random_perturb_z;
        break;

      case 1: // (+x, -y, -z)
        result(0) = max(0) + x_length * random_perturb_x;
        result(1) = min(1) - y_length * random_perturb_y;
        result(2) = min(2) - z_length * random_perturb_z;
        break;

      case 2: // (-x, +y, -z)
        result(0) = min(0) - x_length * random_perturb_x;
        result(1) = max(1) + y_length * random_perturb_y;
        result(2) = min(2) - z_length * random_perturb_z;
        break;

      case 3: // (+x, +y, -z)
        result(0) = max(0) + x_length * random_perturb_x;
        result(1) = max(1) + y_length * random_perturb_y;
        result(2) = min(2) - z_length * random_perturb_z;
        break;

      case 4: // (-x, -y, +z)
        result(0) = min(0) - x_length * random_perturb_x;
        result(1) = min(1) - y_length * random_perturb_y;
        result(2) = max(2) + z_length * random_perturb_z;
        break;

      case 5: // (+x, -y, +z)
        result(0) = max(0) + x_length * random_perturb_x;
        result(1) = min(1) - y_length * random_perturb_y;
        result(2) = max(2) + z_length * random_perturb_z;
        break;

      case 6: // (-x, +y, +z)
        result(0) = min(0) - x_length * random_perturb_x;
        result(1) = max(1) + y_length * random_perturb_y;
        result(2) = max(2) + z_length * random_perturb_z;
        break;

      case 7: // (+x, +y, +z) (default)
      default:
        result(0) = max(0) + x_length * random_perturb_x;
        result(1) = max(1) + y_length * random_perturb_y;
        result(2) = max(2) + z_length * random_perturb_z;
        break;
    }
  }

  void setBinning(const int target_per_bin) override
  {
    const size_t no_tris = _num_triangles;
    double x_range = _bounds.max()(0) - _bounds.min()(0);
    double y_range = _bounds.max()(1) - _bounds.min()(1);
    double z_range = _bounds.max()(2) - _bounds.min()(2);

    if (_binning_type == BinningType::Type::NOBIN)
    {
      if (x_range <= y_range && x_range <= z_range)
        _binning_type = BinningType::Type::YZ_2D;
      else if (y_range <= x_range && y_range <= z_range)
        _binning_type = BinningType::Type::ZX_2D;
      else
        _binning_type = BinningType::Type::XY_2D;
    }

    const int bins_per_dir = std::max(int(std::sqrt(no_tris / double(target_per_bin))), 1);

    binning_2d(bins_per_dir, bins_per_dir);
  }

  void binning_2d(int no_bins_dir_1, int no_bins_dir_2)
  {
    // Determine binning directions based on _binning_type
    int bin_dir_1 = -1, bin_dir_2 = -1;
    if (_binning_type == BinningType::Type::XY_2D)
    {
      bin_dir_1 = 0;
      bin_dir_2 = 1;
    }
    else if (_binning_type == BinningType::Type::YZ_2D)
    {
      bin_dir_1 = 1;
      bin_dir_2 = 2;
    }
    else if (_binning_type == BinningType::Type::ZX_2D)
    {
      bin_dir_1 = 2;
      bin_dir_2 = 0;
    }
    else
    {
      mooseError("Unsupported binning_type for 2D binning");
    }

    const double min1 = _bounds.min()(bin_dir_1);
    const double max1 = _bounds.max()(bin_dir_1);
    const double min2 = _bounds.min()(bin_dir_2);
    const double max2 = _bounds.max()(bin_dir_2);

    const double bin_width_1 = (max1 - min1) / no_bins_dir_1;
    const double bin_width_2 = (max2 - min2) / no_bins_dir_2;
    const double eps1 = 0.01 * bin_width_1;
    const double eps2 = 0.01 * bin_width_2;

    _bins_2d.clear();
    _bins_2d.resize(no_bins_dir_1);
    for (auto & bins_2d_dir2 : _bins_2d)
      bins_2d_dir2.resize(no_bins_dir_2);

    for (unsigned int i = 0; i < static_cast<unsigned int>(no_bins_dir_1); ++i)
    {
      for (unsigned int j = 0; j < static_cast<unsigned int>(no_bins_dir_2); ++j)
      {
        _bins_2d[i][j].first.resize(4);
        _bins_2d[i][j].first[0] = min1 + bin_width_1 * i - eps1;
        _bins_2d[i][j].first[1] = min1 + bin_width_1 * (i + 1) + eps1;
        _bins_2d[i][j].first[2] = min2 + bin_width_2 * j - eps2;
        _bins_2d[i][j].first[3] = min2 + bin_width_2 * (j + 1) + eps2;
        _bins_2d[i][j].second.reserve(
            static_cast<std::size_t>(_num_triangles / no_bins_dir_1 / no_bins_dir_2 * 4));
      }
    }

    for (unsigned int tri_id = 0; tri_id < static_cast<unsigned int>(_num_triangles); ++tri_id)
    {
      const auto & tri = _stl->triangles[tri_id];

      const double c0_1 = tri.p1(bin_dir_1);
      const double c1_1 = tri.p2(bin_dir_1);
      const double c2_1 = tri.p3(bin_dir_1);
      const double cmin1 = std::min({c0_1, c1_1, c2_1});
      const double cmax1 = std::max({c0_1, c1_1, c2_1});

      const double c0_2 = tri.p1(bin_dir_2);
      const double c1_2 = tri.p2(bin_dir_2);
      const double c2_2 = tri.p3(bin_dir_2);
      const double cmin2 = std::min({c0_2, c1_2, c2_2});
      const double cmax2 = std::max({c0_2, c1_2, c2_2});

      unsigned int cbin_min1 = static_cast<unsigned int>((cmin1 - eps1 - min1) / bin_width_1);
      unsigned int cbin_max1 = static_cast<unsigned int>((cmax1 + eps1 - min1) / bin_width_1);
      unsigned int cbin_min2 = static_cast<unsigned int>((cmin2 - eps2 - min2) / bin_width_2);
      unsigned int cbin_max2 = static_cast<unsigned int>((cmax2 + eps2 - min2) / bin_width_2);

      if (cbin_max1 >= static_cast<unsigned int>(no_bins_dir_1))
        cbin_max1 = no_bins_dir_1 - 1;
      if (cbin_max2 >= static_cast<unsigned int>(no_bins_dir_2))
        cbin_max2 = no_bins_dir_2 - 1;

      for (unsigned int i = cbin_min1; i <= cbin_max1; ++i)
        for (unsigned int j = cbin_min2; j <= cbin_max2; ++j)
          _bins_2d[i][j].second.emplace_back(tri_id);
    }
  }

  void generateRayStartAndBinIndex(const Point point,
                                   Point & starting_point, // pass by reference
                                   unsigned int binno[2]) const
  {
    const auto & min = _bounds.min();
    const auto & max = _bounds.max();

    // Copy input point to starting point
    starting_point = point;

    // Determine ray direction and binning directions
    int ray_dir = (_binning_type + 2) % 3;
    int bin_dir_1 = (_binning_type) % 3;
    int bin_dir_2 = (_binning_type + 1) % 3;

    // Extend ray in the opposite direction of ray_dir
    double ray_length = max(ray_dir) - min(ray_dir);
    starting_point(ray_dir) = min(ray_dir) - ray_length;

    // Compute bin widths for both binning directions
    int no_bins_dir1 = static_cast<int>(_bins_2d.size());
    int no_bins_dir2 = static_cast<int>(_bins_2d[0].size());

    double bin_width_dir1 = (max(bin_dir_1) - min(bin_dir_1)) / no_bins_dir1;
    double bin_width_dir2 = (max(bin_dir_2) - min(bin_dir_2)) / no_bins_dir2;

    // Compute bin indices
    binno[0] = static_cast<unsigned int>((point(bin_dir_1) - min(bin_dir_1)) / bin_width_dir1);
    binno[1] = static_cast<unsigned int>((point(bin_dir_2) - min(bin_dir_2)) / bin_width_dir2);
  }

  InsideType::Type
  traceRayAgainstGeometry(const Point ray_start,
                          const Point ray_end /*the point we want to determine in-out*/,
                          const unsigned int * binno = nullptr) const
  {
    if (ifOutsideBoundingBox(ray_end))
      return InsideType::OUTSIDE;

    int intersections = 0;
    Point dir = ray_end - ray_start;

    std::vector<unsigned int> triIDS;
    if (!binno)
    {
      triIDS.clear();
      triIDS.resize(_num_triangles);
      std::iota(std::begin(triIDS), std::end(triIDS), 0);
    }
    else
    {
      triIDS = _bins_2d[binno[0]][binno[1]].second;
    }

    for (const auto & triID : triIDS)
    {
      const auto & tri = _stl->triangles[triID];

      // tri.print();

      ///  Determine if the ray intersects with the bounding box/sphere of a triangle, skipped if not
      if (OutsideRayBBOX(
              ray_start, dir, tri.boundingSphere().center, tri.boundingSphere().radius) ||
          OutsideBoundingSphereOrCircle(
              ray_start, dir, tri.boundingSphere().center, tri.boundingSphere().radius))
        continue;

      // std::cout << "after initial check" << std::endl;

      // std::cout << "ray_start = ";
      // ray_start.print();
      // std::cout << "\n";

      // std::cout << "dir = ";
      // dir.print();

      IntersectionType::Type result_from_ifIntersect =
          ifLineIntersectTriangle(ray_start, dir, tri.v);

      // std::cout << "result_from_ifIntersect = " << result_from_ifIntersect << std::endl;

      switch (result_from_ifIntersect)
      {
        case IntersectionType::ON_GEO_DIM_MINUS_1: // ON_EDGE
        case IntersectionType::ON_STARTPOINT:
          return InsideType::UNKNOWN;

        case IntersectionType::ON_GEO_DIM: // ON_SURFACE
          return InsideType::ON_SURFACE;

        case IntersectionType::INTERSECT:
          ++intersections;
          continue;

        default:
          // do nothing
          continue;
      }
    }

    // std::cout << "intersections = " << intersections << std::endl;

    return (intersections % 2 != 0) ? InsideType::INSIDE /*odd*/
                                    : InsideType::OUTSIDE /*even*/;
  }

  IntersectionType::Type ifLineIntersectTriangle(const Point & orig,
                                                 const Point & dir,
                                                 const std::vector<Point> & vert) const
  {
    assert(vert.size() == 3); // safety check

    const auto & vert0 = vert[0];
    const auto & vert1 = vert[1];
    const auto & vert2 = vert[2];

    Point edge1 = vert1 - vert0;
    Point edge2 = vert2 - vert0;

    Point pvec = dir.cross(edge2);
    double det = edge1 * pvec;

    const double epsilon_edge = 1e-7;
    const double epsilon_surface = 1e-7;

    if (std::abs(det) < 1e-10)
      return IntersectionType::PARALLEL;

    const double inv_det = 1.0 / det;
    Point tvec = orig - vert0;
    double u = tvec * pvec * inv_det;

    if (u < -epsilon_edge || u > 1.0 + epsilon_edge)
      return IntersectionType::NO_INTERSECTION;

    Point qvec = tvec.cross(edge1);
    double v = dir * qvec * inv_det;

    if (v < -epsilon_edge || u + v > 1.0 + epsilon_edge)
      return IntersectionType::NO_INTERSECTION;

    double t = edge2 * qvec * inv_det;

    if (t < 0 || t > 1.0 + epsilon_surface)
      return IntersectionType::NO_INTERSECTION;

    // Classify the intersection type
    const bool near_edge = (u < epsilon_edge || v < epsilon_edge || (u + v) > 1.0 - epsilon_edge);
    const bool near_surface = (t > 1.0 - epsilon_surface);

    if (near_edge)
    {
      if (near_surface)
        return IntersectionType::ON_GEO_DIM;
      else
        return IntersectionType::ON_GEO_DIM_MINUS_1;
    }

    if (near_surface)
      return IntersectionType::ON_GEO_DIM;

    return IntersectionType::INTERSECT;
  }
};

struct RayTracer2D : public RayTracerBase
{
  RayTracer2D(const Line2DData * line2d_data, const int num_lines)
    : line2d_data_(line2d_data), num_lines_(num_lines)
  {
    _dim = 2;
    _binning_type = BinningType::Type::NOBIN;
    _ray_starts.resize(4);
    _bounds = calcBoundingBox();
    for (int i = 0; i < 4; ++i)
    {
      generateRay(i, _ray_starts[i]);
    }
    setBinning(50);
  }

  bool ifInside(const Point & point) override
  {
    if (_binning_type == BinningType::Type::X_1D || _binning_type == BinningType::Type::Y_1D)
    {
      Point ray_start;
      int binno = generateRayStartAndBinIndex(point, ray_start);

      InsideType::Type res = traceRayAgainstGeometry(ray_start, point, binno);

      if (res == InsideType::UNKNOWN)
      {
        for (int i = 0; i < 4; ++i)
        {
          res = traceRayAgainstGeometry(_ray_starts[i], point) /*no binno*/;
          if (res == InsideType::UNKNOWN)
          {
            continue;
          }
          return res;
        }
        return InsideType::UNKNOWN;
      }
      return res;
    }
    else
    {
      mooseError("_binning_type not set. Call setBinning() first.");
      return false;
    }
  }

private:
  const Line2DData * line2d_data_;
  const int num_lines_;

  std::vector<std::pair<std::vector<double>, std::vector<unsigned int>>> _bins_1d;

  BoundingBox calcBoundingBox() override
  {
    // Initialize bounding box with the first point
    Point min_pt = line2d_data_->lines[0].p1;
    Point max_pt = line2d_data_->lines[0].p1;

    for (int i = 0; i < num_lines_; ++i)
    {
      const Point pts[2] = {line2d_data_->lines[i].p1, line2d_data_->lines[i].p2};
      for (const auto & pt : pts)
      {
        for (unsigned int d = 0; d < 3; ++d)
        {
          min_pt(d) = std::min(min_pt(d), pt(d));
          max_pt(d) = std::max(max_pt(d), pt(d));
        }
      }
    }

    // Slightly expand the bounding box to avoid exact edge alignment issues
    const Real eps = 1e-6;
    for (unsigned int d = 0; d < 3; ++d)
    {
      min_pt(d) -= eps;
      max_pt(d) += eps;
    }

    return BoundingBox(min_pt, max_pt);
  }

  void generateRay(int pick, Point & result) override
  {
    const auto & min = _bounds.min();
    const auto & max = _bounds.max();

    const Real x_length = max(0) - min(0);
    const Real y_length = max(1) - min(1);

    // Generate small random perturbations > 1 to avoid edge alignment
    const Real random_perturb_x = 1.0 + static_cast<Real>(std::rand()) / RAND_MAX;
    const Real random_perturb_y = 1.0 + static_cast<Real>(std::rand()) / RAND_MAX;

    switch (pick)
    {
      case 0: // Bottom-left
        result(0) = min(0) - x_length * random_perturb_x;
        result(1) = min(1) - y_length * random_perturb_y;
        break;

      case 1: // Bottom-right
        result(0) = max(0) + x_length * random_perturb_x;
        result(1) = min(1) - y_length * random_perturb_y;
        break;

      case 2: // Top-left
        result(0) = min(0) - x_length * random_perturb_x;
        result(1) = max(1) + y_length * random_perturb_y;
        break;

      case 3: // Top-right (default)
      default:
        result(0) = max(0) + x_length * random_perturb_x;
        result(1) = max(1) + y_length * random_perturb_y;
        break;
    }
  }

  void setBinning(const int target_per_bin) override
  {
    const Real x_range = _bounds.max()(0) - _bounds.min()(0);
    const Real y_range = _bounds.max()(1) - _bounds.min()(1);

    // Only set _binning_type if currently NOBIN
    if (_binning_type == BinningType::Type::NOBIN)
    {
      if (x_range <= y_range)
        _binning_type = BinningType::Type::Y_1D;
      else
        _binning_type = BinningType::Type::X_1D;
    }

    // Call binning_1d with number of bins based on number of lines
    binning_1d(static_cast<unsigned int>(std::ceil(num_lines_ / target_per_bin)));
  }

  void binning_1d(int no_bins_1d)
  {
    // Determine binning direction: 0 for X, 1 for Y
    int bin_dir_1d = -1;
    if (_binning_type == BinningType::Type::X_1D || _binning_type == BinningType::Type::Y_1D)
    {
      bin_dir_1d = static_cast<int>(_binning_type);
    }
    else
    {
      mooseError("_binning_type is not X_1D or Y_1D");
    }

    // Bin size and overlap epsilon
    const double min_val = _bounds.min()(bin_dir_1d);
    const double max_val = _bounds.max()(bin_dir_1d);
    const double bin_width = (max_val - min_val) / no_bins_1d;
    const double eps = 0.01 * bin_width;

    _bins_1d.resize(no_bins_1d);

    for (unsigned int i = 0; i < static_cast<unsigned int>(no_bins_1d); ++i)
    {
      _bins_1d[i].first.resize(2);
      _bins_1d[i].first[0] = min_val + bin_width * i - eps;
      _bins_1d[i].first[1] = min_val + bin_width * (i + 1) + eps;
      _bins_1d[i].second.reserve(static_cast<std::size_t>(num_lines_ / no_bins_1d * 1.5));
    }

    for (unsigned int line_id = 0; line_id < static_cast<unsigned int>(num_lines_); ++line_id)
    {
      const auto & line = line2d_data_->lines[line_id];
      const double c0 = line.p1(bin_dir_1d);
      const double c1 = line.p2(bin_dir_1d);
      const double cmin = std::min(c0, c1);
      const double cmax = std::max(c0, c1);

      unsigned int cbin_min = static_cast<unsigned int>((cmin - eps - min_val) / bin_width);
      unsigned int cbin_max = static_cast<unsigned int>((cmax + eps - min_val) / bin_width);

      // Clamp max index to avoid overflow
      if (cbin_max >= static_cast<unsigned int>(no_bins_1d))
        cbin_max = no_bins_1d - 1;

      // Optional: can downgrade assert to warning or remove in release
      assert(cmin > _bins_1d[cbin_min].first[0]);
      assert(cmin < _bins_1d[cbin_min].first[1]);
      assert(cmax > _bins_1d[cbin_max].first[0]);
      assert(cmax < _bins_1d[cbin_max].first[1]);
      assert(cbin_min <= cbin_max);

      // Assign line to all overlapping bins
      for (unsigned int bin_no = cbin_min; bin_no <= cbin_max; ++bin_no)
        _bins_1d[bin_no].second.emplace_back(line_id);
    }
  }

  int
  generateRayStartAndBinIndex(const Point point,
                              Point & starting_point /*pass by reference: we want to change*/) const
  {
    const auto & min = _bounds.min();
    const auto & max = _bounds.max();

    // Copy input point as ray start
    starting_point(0) = point(0);
    starting_point(1) = point(1);

    // Determine ray direction: if binning is along X (0), ray goes in Y (1), and vice versa
    int ray_dir = (_binning_type + 1) % 2;

    // Extend the ray in the opposite direction (outside the bounding box)
    double ray_length = max(ray_dir) - min(ray_dir);
    starting_point(ray_dir) = min(ray_dir) - ray_length;

    // Determine which bin the ray end lies in
    int num_bins = static_cast<int>(_bins_1d.size());
    double bin_width = (max(_binning_type) - min(_binning_type)) / num_bins;

    int binno = static_cast<int>((point(_binning_type) - min(_binning_type)) / bin_width);
    return binno;
  }

  InsideType::Type
  traceRayAgainstGeometry(const Point ray_start,
                          const Point ray_end /*the point we want to determine in-out*/,
                          int binno = -1) const
  {
    if (ifOutsideBoundingBox(ray_end))
      return InsideType::OUTSIDE;

    int intersections = 0;
    Point dir = ray_end - ray_start;

    // collect lines from bin or use all lines
    std::vector<unsigned int> line_ids;
    if (binno < 0)
    {
      line_ids.resize(num_lines_);
      std::iota(line_ids.begin(), line_ids.end(), 0);
    }
    else
    {
      line_ids = _bins_1d[binno].second;
    }

    for (const auto & line_id : line_ids) /*loop over lines*/
    {
      const auto & line = line2d_data_->lines[line_id];

      // optional bounding circle test to skip non-intersecting lines
      if (OutsideRayBBOX(
              ray_start, dir, line.boundingCircle().center, line.boundingCircle().radius))
        continue;

      if (OutsideBoundingSphereOrCircle(
              ray_start, dir, line.boundingCircle().center, line.boundingCircle().radius))
        continue;

      IntersectionType::Type result = ifLineIntersectLine(ray_start, ray_end, line.p1, line.p2);

      if (result == IntersectionType::ON_GEO_DIM_MINUS_1 ||
          result == IntersectionType::ON_STARTPOINT)
        return InsideType::UNKNOWN;

      if (result == IntersectionType::ON_GEO_DIM)
        return InsideType::ON_SURFACE;

      if (result == IntersectionType::INTERSECT)
        ++intersections;
    }

    return (intersections % 2 != 0) ? InsideType::INSIDE /*odd*/
                                    : InsideType::OUTSIDE /*even*/;
  }

  IntersectionType::Type ifLineIntersectLine(const Point orig,
                                             const Point dest,
                                             const Point vert0,
                                             const Point vert1) const
  {
    // Line AB represented as a1x + b1y = c1
    double a1 = dest(1) - orig(1);
    double b1 = orig(0) - dest(0);
    double c1 = a1 * orig(0) + b1 * orig(1);

    // Line CD represented as a2x + b2y = c2
    double a2 = vert0(1) - vert1(1);
    double b2 = vert1(0) - vert0(0);
    double c2 = a2 * vert1(0) + b2 * vert1(1);

    double det = a1 * b2 - a2 * b1;
    const double epsilON_GEO_DIM_MINUS_1 = 1e-7;
    const double epsilon_edge = 1e-7;

    if (std::abs(det) < 1e-10)
      return IntersectionType::PARALLEL;
    else
    {
      double x = (b2 * c1 - b1 * c2) / det;
      double y = (a1 * c2 - a2 * c1) / det;

      // Check if intersection is outside both segments
      if (x > std::max(orig(0), dest(0)) + epsilon_edge ||
          y > std::max(orig(1), dest(1)) + epsilon_edge ||
          x > std::max(vert0(0), vert1(0)) + epsilON_GEO_DIM_MINUS_1 ||
          y > std::max(vert0(1), vert1(1)) + epsilON_GEO_DIM_MINUS_1 ||
          x < std::min(orig(0), dest(0)) - epsilon_edge ||
          y < std::min(orig(1), dest(1)) - epsilon_edge ||
          x < std::min(vert0(0), vert1(0)) - epsilON_GEO_DIM_MINUS_1 ||
          y < std::min(vert0(1), vert1(1)) - epsilON_GEO_DIM_MINUS_1)
      {
        return IntersectionType::NO_INTERSECTION;
      }
      else
      {
        // If it's on one of the endpoints
        if ((std::fabs(x - vert0(0)) < epsilON_GEO_DIM_MINUS_1 &&
             std::fabs(y - vert0(1)) < epsilON_GEO_DIM_MINUS_1) ||
            (std::fabs(x - vert1(0)) < epsilON_GEO_DIM_MINUS_1 &&
             std::fabs(y - vert1(1)) < epsilON_GEO_DIM_MINUS_1))
        {
          if (std::fabs(x - dest(0)) < epsilon_edge && std::fabs(y - dest(1)) < epsilon_edge)
            return IntersectionType::ON_GEO_DIM;
          else
            return IntersectionType::ON_GEO_DIM_MINUS_1;
        }
        else
        {
          if (std::fabs(x - dest(0)) < epsilon_edge && std::fabs(y - dest(1)) < epsilon_edge)
            return IntersectionType::ON_GEO_DIM;
          else
            return IntersectionType::INTERSECT;
        }
      }
    }
  }
};

class SubdomainInterceptedGeneratorWaterTightGeo : public MeshGenerator
{

public:
  static InputParameters validParams();
  SubdomainInterceptedGeneratorWaterTightGeo(const InputParameters & parameters);

  std::unique_ptr<libMesh::MeshBase> generate() override;

protected:
  std::unique_ptr<libMesh::MeshBase> & _input;

protected:
  /// IDs for subdomain classification
  SubdomainID _subdomain_id_inside;
  SubdomainID _subdomain_id_outside;
  SubdomainID _subdomain_id_intercepted;
  SubdomainID _subdomain_id_false_intercected;
  SubdomainID _subdomain_id_neighbor_intercepted;
  SubdomainID _subdomain_id_original_intercepted;

  /// Threshold value for classification
  Real _threshold;

  /// Lambda parameter for false intersection classification
  Real _lambda;

  /// Outer boundary handling flag
  bool _outer_boundary;

  /// MarkNeighborOfIntercept
  bool _mark_neighbor_of_intercepted;

  bool _random_mark_some_intercepted_as_original_intercepted;
  bool _multi_geo;

  std::string _water_tight_geo_path;

  Point _shift_geometry;

  int _qrule_order;

  bool _simple_classification;

private:
  Line2DData _line2d_data;
  STLData _stl_data;
  std::unique_ptr<Mesh> _boundary_mesh;

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
  float parse_float(std::ifstream & s);
  Point parse_point(std::ifstream & s);
};
