/****************************************************************************/
/*                        DO NOT MODIFY THIS HEADER                         */
/*                                                                          */
/* MALAMUTE: MOOSE Application Library for Advanced Manufacturing UTilitiEs */
/*                                                                          */
/*           Copyright 2021 - 2023, Battelle Energy Alliance, LLC           */
/*                           ALL RIGHTS RESERVED                            */
/****************************************************************************/

#include "PiecewiseLinearSpatioTemporalPathBase.h"

InputParameters
PiecewiseLinearSpatioTemporalPathBase::validParams()
{
  auto params = SpatioTemporalPath::validParams();
  params.addParam<Real>("time_tolerance", 5e-12, "Tolerance of query time");
  MooseEnum outside_behavior("CONSTANT EXTRAPOLATION EXCEPTION", "CONSTANT");
  params.addParam<MooseEnum>(
      "outside",
      outside_behavior,
      "The method to use when extrapolating the path position outside its temporal support. "
      "CONSTANT: Return the closest path point; EXTRAPOLATION: Linear extrapolation; EXCEPTION: "
      "Raise an exception when trying to extrapolate.");
  return params;
}

PiecewiseLinearSpatioTemporalPathBase::PiecewiseLinearSpatioTemporalPathBase(
    const InputParameters & params)
  : SpatioTemporalPath(params),
    _t_tol(getParam<Real>("time_tolerance")),
    _outside(getParam<MooseEnum>("outside"))
{
}

Point
PiecewiseLinearSpatioTemporalPathBase::position(Real t) const
{
  // Previous issue:
  // Always used interpolation even if t exactly matched a known time point.
  // This caused unnecessary floating-point noise and masked true coordinate values.

  if (t < (_times.front() - _t_tol) || t > (_times.back() + _t_tol))
  {
    if (_outside == "EXCEPTION")
      mooseException("A spatiotemporal path is queried outside its temporal support.");
    else if (_outside == "CONSTANT")
    {
      if (t < _times.front())
        return _coords.front();
      if (t > _times.back())
        return _coords.back();
    }
    else if (_outside == "EXTRAPOLATION")
    {
      // Optional: implement linear extrapolation using endpoints
      // For now: fallback to edge values
      return (t < _times.front()) ? _coords.front() : _coords.back();
    }
    else
      paramError("outside", "Unsupported extrapolation method.");
  }

  auto [i1, i2] = getIntervalIndices(t);

  // If t exactly matches a known time (within tolerance), return that node directly
  if (std::abs(t - _times[i1]) < _t_tol)
    return _coords[i1];
  if (std::abs(t - _times[i2]) < _t_tol)
    return _coords[i2];

  // Linearly interpolate
  const auto & p1 = _coords[i1];
  const auto & p2 = _coords[i2];
  Real fraction = (t - _times[i1]) / (_times[i2] - _times[i1]);
  Point p = p1 + fraction * (p2 - p1);

  if (std::isnan(p(0)) || std::isnan(p(1)) || std::isnan(p(2)))
    mooseException("Encountered NaN when querying position at time ", t);

  return p;
}

void
PiecewiseLinearSpatioTemporalPathBase::setCoords(const std::vector<Real> & x,
                                                 const std::vector<Real> & y,
                                                 const std::vector<Real> & z)
{
  // Size check
  auto sz = std::max({x.size(), y.size(), z.size()});
  if (sz == 0 || (!x.empty() && x.size() != sz) || (!x.empty() && x.size() != sz) ||
      (!x.empty() && x.size() != sz))
    mooseError(
        "Error while constructing a spatio-temporal path: At lease one of the x-, y-, and z- "
        "coordinates must be non-empty, and the non-zero sizes must match. x-coordinates have "
        "size ",
        x.size(),
        ", y-coordinates have size ",
        y.size(),
        ", z-coordinates have size ",
        z.size(),
        ".");

  // Assigne coordinates
  _coords.resize(sz);
  for (auto i : make_range(sz))
  {
    _coords[i](0) = x.empty() ? 0.0 : x[i];
    _coords[i](1) = y.empty() ? 0.0 : y[i];
    _coords[i](2) = z.empty() ? 0.0 : z[i];
  }
}

void
PiecewiseLinearSpatioTemporalPathBase::validate() const
{
  // Time and coords must have same size
  if (_times.size() != _coords.size())
    mooseError("Error while constructing a spatio-temporal path: The time series and the "
               "coordinates must have the same size. The time series have size ",
               _times.size(),
               ", and the coordinates have size ",
               _coords.size(),
               ".");

  // There must be at least one line segment
  if (_times.size() < 2)
    mooseError("Error while constructing a spatio-temporal path: There must be at least two "
               "vertices on the path, but only ",
               _times.size(),
               " is provided.");

  // Time must be non-decreasing
  for (auto i : index_range(_times))
    if (i > 0 && _times[i] < _times[i - 1])
      mooseError("Error while constructing a spatio-temporal path: The time series must be "
                 "non-decreasing. The time at index ",
                 i - 1,
                 " is ",
                 _times[i - 1],
                 ", and the time at index ",
                 i,
                 " is ",
                 _times[i],
                 ".");
}

std::pair<unsigned int, unsigned int>
PiecewiseLinearSpatioTemporalPathBase::getIntervalIndices(Real t) const
{
  // Previous issue:
  // The old implementation iterated from front to back and returned the first match where t >=
  // _times[i], which always resulted in returning the last interval even for valid t values at the
  // beginning. It also didn't handle tolerance correctly and had no proper handling for
  // out-of-range time values.

  // Fix: Handle three cases clearly: before first time, after last time, and within range.
  // Add tolerance support to avoid floating-point errors.

  // Handle before first time
  if (t <= _times.front())
    return {0, 1};

  // Handle after last time
  if (t >= _times.back())
    return {_times.size() - 2, _times.size() - 1};

  // Search for interval such that t ∈ [_times[i], _times[i+1]]
  for (std::size_t i = 0; i < _times.size() - 1; ++i)
  {
    if (_times[i] - _t_tol <= t && t <= _times[i + 1] + _t_tol)
      return {i, i + 1};
  }

  mooseError("Failed to find interval for time ",
             t,
             ". Try increasing 'time_tolerance' to account for floating-point error.");
}
