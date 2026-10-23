//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosInterfaceGatheringOracle.h"

#ifdef MOOSE_KOKKOS_ENABLED
#ifdef NEML2_ENABLED

registerMooseObject("MooseTestApp", KokkosInterfaceGatheringOracle);

namespace
{
// Count the batch rows whose nine row-major components match either expected pattern. The
// reduction runs in the build's Kokkos execution space: ExecSpace and MemSpace are macros that
// resolve at compile time to the single device this build targets (CUDA/HIP/SYCL on a GPU build,
// OpenMP host otherwise). NEML2 places the gathered buffer on that same device, so wrapping the
// raw pointer in a MemSpace view reads it where it lives, with no host copy, mirror, or .cpu()
// staging. The expected patterns are copied into device-copyable Kokkos::Array captured by value,
// so no host pointer is dereferenced on the device.
std::size_t
countMatchingRows(const double * ptr,
                  const std::size_t total_batch,
                  const std::array<Real, 9> & expected_primary,
                  const std::array<Real, 9> & expected_neighbor)
{
  Kokkos::View<const double *, MemSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>> buffer(
      ptr, total_batch * 9);

  Kokkos::Array<double, 9> exp_p;
  Kokkos::Array<double, 9> exp_n;
  for (unsigned int k = 0; k < 9; ++k)
  {
    exp_p[k] = expected_primary[k];
    exp_n[k] = expected_neighbor[k];
  }

  // The gathered values and the expected parameters are the same IEEE-754 doubles, so the squared
  // component difference is compared against a tight squared tolerance (fabs is avoided to keep the
  // kernel free of device math-library calls).
  constexpr double tol2 = 1e-14 * 1e-14;

  std::size_t checked = 0;
  Kokkos::parallel_reduce(
      "KokkosInterfaceGatheringOracle",
      Kokkos::RangePolicy<ExecSpace>(0, total_batch),
      KOKKOS_LAMBDA(const std::size_t row, std::size_t & local_checked) {
        bool primary_match = true;
        bool neighbor_match = true;
        for (unsigned int k = 0; k < 9; ++k)
        {
          const double v = buffer(row * 9 + k);
          const double dp = v - exp_p[k];
          const double dn = v - exp_n[k];
          if (dp * dp >= tol2)
            primary_match = false;
          if (dn * dn >= tol2)
            neighbor_match = false;
        }
        if (primary_match || neighbor_match)
          local_checked += 1;
      },
      checked);
  Kokkos::fence();
  return checked;
}
}

InputParameters
KokkosInterfaceGatheringOracle::validParams()
{
  auto params = GeneralUserObject::validParams();
  params += Postprocessor::validParams();
  params.addClassDescription(
      "Test-only postprocessor: verifies on the device that a KokkosInterfaceQuantityToNEML2 "
      "gatherer wrote the expected full rank-two components for both interface sides. Returns the "
      "number of batch rows matching an expected side; a wrong value fails the CSV comparison.");
  params.addRequiredParam<UserObjectName>("gatherer",
                                          "The KokkosInterfaceQuantityToNEML2 to verify.");
  params.addRequiredParam<UserObjectName>("batch_index_generator",
                                          "The NEML2BatchIndexGenerator used by the gatherer.");
  params.addRequiredParam<std::vector<Real>>(
      "expected_primary",
      "Expected nine primary-side rank-two components in row-major order (i * 3 + j).");
  params.addRequiredParam<std::vector<Real>>(
      "expected_neighbor",
      "Expected nine neighbor-side rank-two components in row-major order (i * 3 + j).");
  return params;
}

KokkosInterfaceGatheringOracle::KokkosInterfaceGatheringOracle(const InputParameters & parameters)
  : GeneralUserObject(parameters),
    Postprocessor(this),
    _gatherer(getUserObject<KokkosInterfaceQuantityToNEML2>("gatherer")),
    _batch_index_generator(getUserObject<NEML2BatchIndexGenerator>("batch_index_generator"))
{
  const auto & primary = getParam<std::vector<Real>>("expected_primary");
  const auto & neighbor = getParam<std::vector<Real>>("expected_neighbor");
  if (primary.size() != 9 || neighbor.size() != 9)
    paramError(primary.size() != 9 ? "expected_primary" : "expected_neighbor",
               "Expected exactly nine row-major rank-two components.");
  std::copy(primary.begin(), primary.end(), _expected_primary.begin());
  std::copy(neighbor.begin(), neighbor.end(), _expected_neighbor.begin());
}

void
KokkosInterfaceGatheringOracle::execute()
{
  const auto data = _gatherer.gatheredData();
  if (!data.defined())
    mooseError("KokkosInterfaceGatheringOracle: gatheredData() is undefined; "
               "ensure execute_on aligns so the gatherer finalizes before the oracle executes.");

  // Shape: {total_batch, 3, 3}, row-major doubles, resident on the NEML2 device.
  const auto total_batch = static_cast<std::size_t>(data.size(0));
  if (total_batch == 0)
  {
    _checked_rows = 0;
    return;
  }

  const auto * ptr = data.const_data_ptr<double>();

  _checked_rows = countMatchingRows(ptr, total_batch, _expected_primary, _expected_neighbor);
}

PostprocessorValue
KokkosInterfaceGatheringOracle::getValue() const
{
  return static_cast<PostprocessorValue>(_checked_rows);
}

#endif // NEML2_ENABLED
#endif // MOOSE_KOKKOS_ENABLED
