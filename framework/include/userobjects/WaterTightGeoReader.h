#pragma once

#include "GeneralUserObject.h"
#include <memory>
#include <vector>
#include <string>
#include "KDTree.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_base.h"

namespace InOutTest
{
enum class DistanceType
{
  NONE = -1,
  SIGN_DISTANCE = 0,
  GEOMETRY = 1
};
}

namespace BinningType
{
enum class Binning
{
  NOBIN = -1,
  X_1D = 0,
  Y_1D = 1,
  Z_1D = 2,
  XY_2D = 3,
  YZ_2D = 4,
  ZX_2D = 5
};
}

namespace InsideType
{
enum class InOutType
{
  UNKNOWN = -1,
  OUTSIDE = 0,
  INSIDE = 1,
  ON_SURFACE = 2
};
}

namespace IntersectionType
{
enum class InterType
{
  INTERSECT = 0,
  PARALLEL = 1,
  ON_GEO_DIM_MINUS_1 = 2,
  ON_GEO_DIM = 3,
  NO_INTERSECTION = 4,
  ON_STARTPOINT = 5
};
}

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

class RayTracerBase
{
public:
  RayTracerBase(const Mesh * boundary_mesh) : _bd_mesh(boundary_mesh) {}

  virtual ~RayTracerBase() = default;
  virtual bool ifInside(const Point & p) = 0;

protected:
  int _dim = -1;
  const Mesh * _bd_mesh;
  BoundingBox _bounds;
  std::vector<Point> _ray_starts;
  BinningType::Binning _binning_type;

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

  BoundingBox calcBoundingBox()
  {
    // Make sure mesh is not null and has elements
    if (!_bd_mesh || _bd_mesh->n_elem() == 0)
      mooseError("RayTracer: Boundary mesh is empty or not initialized.");

    // Initialize with the bounding box of the first element
    auto elem_it = _bd_mesh->active_elements_begin(); // not local_elements
    const Elem * first_elem = *elem_it;
    BoundingBox bbox = first_elem->loose_bounding_box();

    // Expand the bounding box to include all active elements
    for (++elem_it; elem_it != _bd_mesh->active_elements_end(); ++elem_it)
    {
      const Elem * elem = *elem_it;
      bbox.union_with(elem->loose_bounding_box());
    }

    // Slightly expand the final bounding box to avoid edge alignment issues
    const Real eps = 1e-6;
    Point min_pt = bbox.min();
    Point max_pt = bbox.max();

    for (unsigned int d = 0; d < 3; ++d)
    {
      min_pt(d) -= eps;
      max_pt(d) += eps;
    }

    return BoundingBox(min_pt, max_pt);
  }
};

struct RayTracer2D : public RayTracerBase
{
  RayTracer2D(const Mesh * boundary_mesh)
    : RayTracerBase(boundary_mesh), num_lines_(boundary_mesh->n_elem())
  {
    _dim = 2;
    _binning_type = BinningType::Binning::NOBIN;
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
    if (_binning_type == BinningType::Binning::X_1D || _binning_type == BinningType::Binning::Y_1D)
    {
      Point ray_start;
      int binno = generateRayStartAndBinIndex(point, ray_start);

      InsideType::InOutType res = traceRayAgainstGeometry(ray_start, point, binno);

      if (res == InsideType::InOutType::UNKNOWN)
      {
        for (int i = 0; i < 4; ++i)
        {
          res = traceRayAgainstGeometry(_ray_starts[i], point) /*no binno*/;
          if (res == InsideType::InOutType::UNKNOWN)
          {
            continue;
          }
          return res == InsideType::InOutType::INSIDE;
        }
        return false;
      }
      return res == InsideType::InOutType::INSIDE;
    }
    else
    {
      mooseError("_binning_type not set. Call setBinning() first.");
      return false;
    }
  }

private:
  const int num_lines_;

  std::vector<std::pair<std::vector<double>, std::vector<unsigned int>>> _bins_1d;

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
    if (_binning_type == BinningType::Binning::NOBIN)
    {
      if (x_range <= y_range)
        _binning_type = BinningType::Binning::Y_1D;
      else
        _binning_type = BinningType::Binning::X_1D;
    }

    // Call binning_1d with number of bins based on number of lines
    binning_1d(static_cast<unsigned int>(std::ceil(num_lines_ / target_per_bin)));
  }

  void binning_1d(int no_bins_1d)
  {
    // Determine binning direction: 0 for X, 1 for Y
    int bin_dir_1d = -1;
    if (_binning_type == BinningType::Binning::X_1D || _binning_type == BinningType::Binning::Y_1D)
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

    int line_id = 0;
    for (const auto & elem : _bd_mesh->element_ptr_range())
    {
      const double c0 = elem->node_ref(0)(bin_dir_1d); /*point on line 1*/
      const double c1 = elem->node_ref(1)(bin_dir_1d); /*point on line 2*/
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

      ++line_id;
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

    int binning_type_int = static_cast<int>(_binning_type);

    // Determine ray direction: if binning is along X (0), ray goes in Y (1), and vice versa
    int ray_dir = (binning_type_int + 1) % 2;

    // Extend the ray in the opposite direction (outside the bounding box)
    double ray_length = max(ray_dir) - min(ray_dir);
    starting_point(ray_dir) = min(ray_dir) - ray_length;

    // Determine which bin the ray end lies in
    int num_bins = static_cast<int>(_bins_1d.size());
    double bin_width = (max(binning_type_int) - min(binning_type_int)) / num_bins;

    int binno = static_cast<int>((point(binning_type_int) - min(binning_type_int)) / bin_width);
    return binno;
  }

  InsideType::InOutType
  traceRayAgainstGeometry(const Point ray_start,
                          const Point ray_end /*the point we want to determine in-out*/,
                          int binno = -1) const
  {
    if (ifOutsideBoundingBox(ray_end))
      return InsideType::InOutType::OUTSIDE;

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

    for (const auto & elem : _bd_mesh->element_ptr_range()) /*loop over lines*/
    {
      auto [center, radius] = computeBoundingCircle(elem);

      // optional bounding circle test to skip non-intersecting lines
      if (OutsideRayBBOX(ray_start, dir, center, radius) or
          OutsideBoundingSphereOrCircle(ray_start, dir, center, radius))
        continue;

      IntersectionType::InterType result =
          ifLineIntersectLine(ray_start, ray_end, elem->node_ref(0), elem->node_ref(1));

      if (result == IntersectionType::InterType::ON_GEO_DIM_MINUS_1 ||
          result == IntersectionType::InterType::ON_STARTPOINT)
        return InsideType::InOutType::UNKNOWN;

      if (result == IntersectionType::InterType::ON_GEO_DIM)
        return InsideType::InOutType::ON_SURFACE;

      if (result == IntersectionType::InterType::INTERSECT)
        ++intersections;
    }

    return (intersections % 2 != 0) ? InsideType::InOutType::INSIDE /*odd*/
                                    : InsideType::InOutType::OUTSIDE /*even*/;
  }

  IntersectionType::InterType ifLineIntersectLine(const Point orig,
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
    const double epsilon_surface = 1e-7;
    const double epsilon_edge = 1e-7;

    if (std::abs(det) < 1e-10)
      return IntersectionType::InterType::PARALLEL;
    else
    {
      double x = (b2 * c1 - b1 * c2) / det;
      double y = (a1 * c2 - a2 * c1) / det;

      // Check if intersection is outside both segments
      if (x > std::max(orig(0), dest(0)) + epsilon_edge ||
          y > std::max(orig(1), dest(1)) + epsilon_edge ||
          x > std::max(vert0(0), vert1(0)) + epsilon_surface ||
          y > std::max(vert0(1), vert1(1)) + epsilon_surface ||
          x < std::min(orig(0), dest(0)) - epsilon_edge ||
          y < std::min(orig(1), dest(1)) - epsilon_edge ||
          x < std::min(vert0(0), vert1(0)) - epsilon_surface ||
          y < std::min(vert0(1), vert1(1)) - epsilon_surface)
      {
        return IntersectionType::InterType::NO_INTERSECTION;
      }
      else
      {
        // If it's on one of the endpoints
        if ((std::fabs(x - vert0(0)) < epsilon_surface &&
             std::fabs(y - vert0(1)) < epsilon_surface) ||
            (std::fabs(x - vert1(0)) < epsilon_surface &&
             std::fabs(y - vert1(1)) < epsilon_surface))
        {
          if (std::fabs(x - dest(0)) < epsilon_edge && std::fabs(y - dest(1)) < epsilon_edge)
            return IntersectionType::InterType::ON_GEO_DIM;
          else
            return IntersectionType::InterType::ON_GEO_DIM_MINUS_1;
        }
        else
        {
          if (std::fabs(x - dest(0)) < epsilon_edge && std::fabs(y - dest(1)) < epsilon_edge)
            return IntersectionType::InterType::ON_GEO_DIM;
          else
            return IntersectionType::InterType::INTERSECT;
        }
      }
    }
  }

  std::pair<Point, Real> computeBoundingCircle(const Elem * edge_elem) const
  {
    libmesh_assert(edge_elem->type() == EDGE2);
    const Point & p1 = edge_elem->point(0);
    const Point & p2 = edge_elem->point(1);
    Point center = edge_elem->vertex_average();
    Real radius = 0.5 * (p2 - p1).norm();
    return {center, radius};
  }
};

struct RayTracer3D : public RayTracerBase
{
  RayTracer3D(const Mesh * boundary_mesh)
    : RayTracerBase(boundary_mesh), _num_triangles(boundary_mesh->n_elem())
  {
    _dim = 3;
    _binning_type = BinningType::Binning::NOBIN;

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

    InsideType::InOutType res = traceRayAgainstGeometry(ray_start, point, binno);

    /// falling back to original ray-tracing
    if (res == InsideType::InOutType::UNKNOWN)
    {
      for (int i = 0; i < 8; i++)
      {
        res = traceRayAgainstGeometry(ray_start, point);
        if (res == InsideType::InOutType::UNKNOWN)
          continue;
        return res == InsideType::InOutType::INSIDE;
      }
      return false;
    }

    return res == InsideType::InOutType::INSIDE;
  }

private:
  const int _num_triangles;

  std::vector<std::vector<std::pair<std::vector<double>, std::vector<unsigned int>>>> _bins_2d;

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

    if (_binning_type == BinningType::Binning::NOBIN)
    {
      if (x_range <= y_range && x_range <= z_range)
        _binning_type = BinningType::Binning::YZ_2D;
      else if (y_range <= x_range && y_range <= z_range)
        _binning_type = BinningType::Binning::ZX_2D;
      else
        _binning_type = BinningType::Binning::XY_2D;
    }

    const int bins_per_dir = std::max(int(std::sqrt(no_tris / double(target_per_bin))), 1);

    binning_2d(bins_per_dir, bins_per_dir);
  }

  // Assign each triangle to all 2D bins (grid cells) that its projected bounding box overlaps.
  // This allows efficient spatial queries by limiting the number of triangles checked per region.
  // For each triangle, compute its 2D bounding box in the selected binning plane (XY, YZ, or ZX),
  // then assign it to all bins that the bounding box overlaps. This enables efficient spatial
  // lookup.
  void binning_2d(const int no_bins_dir_1, const int no_bins_dir_2)
  {
    // Determine binning directions based on _binning_type
    int bin_dir_1 = -1, bin_dir_2 = -1;
    if (_binning_type == BinningType::Binning::XY_2D)
    {
      bin_dir_1 = 0;
      bin_dir_2 = 1;
    }
    else if (_binning_type == BinningType::Binning::YZ_2D)
    {
      bin_dir_1 = 1;
      bin_dir_2 = 2;
    }
    else if (_binning_type == BinningType::Binning::ZX_2D)
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

    int tri_id = 0;
    for (const auto & elem : _bd_mesh->element_ptr_range())
    {

      const double c0_1 = elem->node_ref(0)(bin_dir_1);
      const double c1_1 = elem->node_ref(1)(bin_dir_1);
      const double c2_1 = elem->node_ref(2)(bin_dir_1);
      const double cmin1 = std::min({c0_1, c1_1, c2_1});
      const double cmax1 = std::max({c0_1, c1_1, c2_1});

      const double c0_2 = elem->node_ref(0)(bin_dir_2);
      const double c1_2 = elem->node_ref(1)(bin_dir_2);
      const double c2_2 = elem->node_ref(2)(bin_dir_2);
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
      ++tri_id;
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

    int binning_type_int = static_cast<int>(_binning_type);

    // Determine ray direction and binning directions
    int ray_dir = (binning_type_int + 2) % 3;
    int bin_dir_1 = (binning_type_int) % 3;
    int bin_dir_2 = (binning_type_int + 1) % 3;

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

  InsideType::InOutType
  traceRayAgainstGeometry(const Point ray_start,
                          const Point ray_end /*the point we want to determine in-out*/,
                          const unsigned int * binno = nullptr) const
  {
    if (ifOutsideBoundingBox(ray_end))
      return InsideType::InOutType::OUTSIDE;

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
      const auto & tri_elem = _bd_mesh->elem_ptr(triID);

      auto [center, radius] = computeBoundingSphere(tri_elem);

      ///  Determine if the ray intersects with the bounding box/sphere of a triangle, skipped if not
      if (OutsideRayBBOX(ray_start, dir, center, radius) ||
          OutsideBoundingSphereOrCircle(ray_start, dir, center, radius))
        continue;

      IntersectionType::InterType result_from_ifIntersect = ifLineIntersectTriangle(
          ray_start, dir, {tri_elem->point(0), tri_elem->point(1), tri_elem->point(2)});

      switch (result_from_ifIntersect)
      {
        case IntersectionType::InterType::ON_GEO_DIM_MINUS_1: // ON_EDGE
        case IntersectionType::InterType::ON_STARTPOINT:
          return InsideType::InOutType::UNKNOWN;

        case IntersectionType::InterType::ON_GEO_DIM: // ON_SURFACE
          return InsideType::InOutType::ON_SURFACE;

        case IntersectionType::InterType::INTERSECT:
          ++intersections;
          continue;

        default:
          // do nothing
          continue;
      }
    }

    return (intersections % 2 != 0) ? InsideType::InOutType::INSIDE /*odd*/
                                    : InsideType::InOutType::OUTSIDE /*even*/;
  }

  IntersectionType::InterType ifLineIntersectTriangle(const Point & orig,
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
      return IntersectionType::InterType::PARALLEL;

    const double inv_det = 1.0 / det;
    Point tvec = orig - vert0;
    double u = tvec * pvec * inv_det;

    if (u < -epsilon_edge || u > 1.0 + epsilon_edge)
      return IntersectionType::InterType::NO_INTERSECTION;

    Point qvec = tvec.cross(edge1);
    double v = dir * qvec * inv_det;

    if (v < -epsilon_edge || u + v > 1.0 + epsilon_edge)
      return IntersectionType::InterType::NO_INTERSECTION;

    double t = edge2 * qvec * inv_det;

    if (t < 0 || t > 1.0 + epsilon_surface)
      return IntersectionType::InterType::NO_INTERSECTION;

    // Classify the intersection type
    const bool near_edge = (u < epsilon_edge || v < epsilon_edge || (u + v) > 1.0 - epsilon_edge);
    const bool near_surface = (t > 1.0 - epsilon_surface);

    if (near_edge)
    {
      if (near_surface)
        return IntersectionType::InterType::ON_GEO_DIM;
      else
        return IntersectionType::InterType::ON_GEO_DIM_MINUS_1;
    }

    if (near_surface)
      return IntersectionType::InterType::ON_GEO_DIM;

    return IntersectionType::InterType::INTERSECT;
  }

  std::pair<Point, Real> computeBoundingSphere(const Elem * tri_elem) const
  {
    libmesh_assert(tri_elem->n_nodes() == 3);

    // Compute centroid of triangle
    const Point & centroid = tri_elem->vertex_average();

    const Point & A = tri_elem->point(0);
    const Point & B = tri_elem->point(1);
    const Point & C = tri_elem->point(2);

    // Compute maximum squared distance from center to any vertex
    Real max_sq_dist = 0.0;
    for (const Point * p : {&A, &B, &C})
    {
      Real dx = p->operator()(0) - centroid(0);
      Real dy = p->operator()(1) - centroid(1);
      Real dz = p->operator()(2) - centroid(2);
      Real dist_sq = dx * dx + dy * dy + dz * dz;
      if (dist_sq > max_sq_dist)
        max_sq_dist = dist_sq;
    }

    Real radius = std::sqrt(max_sq_dist) * 1.01; // add epsilon buffer
    return {centroid, radius};
  }
};

class WaterTightGeoReader : public GeneralUserObject
{
public:
  static InputParameters validParams();
  WaterTightGeoReader(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void initialize() override {};
  virtual void execute() override {};
  virtual void finalize() override {};

  const std::shared_ptr<KDTree> getKDTree() const { return _kd_tree; }
  const std::shared_ptr<Mesh> getBoundaryMesh() const { return _boundary_mesh; }
  int getDimOriginalMesh() const { return _dim_original_mesh; }
  const std::shared_ptr<std::vector<std::size_t>> getElemIdMap() const { return _elem_id_map; }

protected:
  void buildKDtree();

  std::shared_ptr<Mesh> _boundary_mesh;
  std::string _mesh_file;
  Point _shift_geometry;
  std::shared_ptr<KDTree> _kd_tree;
  std::shared_ptr<std::vector<std::size_t>> _elem_id_map;

private:
  int _dim_original_mesh;
  std::vector<Point> _mid_points;
};
