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

InputParameters
KokkosInterfaceGatheringOracle::validParams()
{
  auto params = GeneralUserObject::validParams();
  params += Postprocessor::validParams();
  params.addClassDescription(
      "Test-only postprocessor: verifies that a KokkosInterfaceQuantityToNEML2 gatherer wrote "
      "the expected diagonal values for both interface sides.  Returns the number of batch rows "
      "checked; a wrong value triggers mooseError.");
  params.addRequiredParam<UserObjectName>("gatherer",
                                          "The KokkosInterfaceQuantityToNEML2 to verify.");
  params.addRequiredParam<UserObjectName>("batch_index_generator",
                                          "The NEML2BatchIndexGenerator used by the gatherer.");
  params.addRequiredParam<Real>(
      "expected_diag_primary",
      "Expected value of every diagonal component F_00, F_11, F_22 on the primary side.");
  params.addRequiredParam<Real>(
      "expected_diag_neighbor",
      "Expected value of every diagonal component F_00, F_11, F_22 on the neighbor side.");
  return params;
}

KokkosInterfaceGatheringOracle::KokkosInterfaceGatheringOracle(const InputParameters & parameters)
  : GeneralUserObject(parameters),
    Postprocessor(this),
    _gatherer(getUserObject<KokkosInterfaceQuantityToNEML2>("gatherer")),
    _batch_index_generator(getUserObject<NEML2BatchIndexGenerator>("batch_index_generator")),
    _expected_diag_primary(getParam<Real>("expected_diag_primary")),
    _expected_diag_neighbor(getParam<Real>("expected_diag_neighbor"))
{
}

void
KokkosInterfaceGatheringOracle::execute()
{
  const auto data = _gatherer.gatheredData();
  if (!data.defined())
    mooseError("KokkosInterfaceGatheringOracle: gatheredData() is undefined; "
               "ensure execute_on aligns so the gatherer finalizes before the oracle executes.");

  // Shape: {total_batch, 3, 3}, row-major doubles, host-resident on CPU Kokkos.
  const auto total_batch = static_cast<std::size_t>(data.size(0));
  if (total_batch == 0)
  {
    _checked_rows = 0;
    return;
  }

  constexpr double tol = 1e-14;
  const auto * ptr = data.const_data_ptr<double>();
  std::size_t checked = 0;
  for (std::size_t row = 0; row < total_batch; ++row)
  {
    const auto * rptr = ptr + row * 9;
    const double d00 = rptr[0]; // F(0,0)
    const double d11 = rptr[4]; // F(1,1)
    const double d22 = rptr[8]; // F(2,2)
    if (std::abs(d00 - _expected_diag_primary) < tol &&
        std::abs(d11 - _expected_diag_primary) < tol &&
        std::abs(d22 - _expected_diag_primary) < tol)
    {
      ++checked;
      continue;
    }
    if (std::abs(d00 - _expected_diag_neighbor) < tol &&
        std::abs(d11 - _expected_diag_neighbor) < tol &&
        std::abs(d22 - _expected_diag_neighbor) < tol)
    {
      ++checked;
      continue;
    }
    mooseError("KokkosInterfaceGatheringOracle: batch row ",
               row,
               " has diagonal (",
               d00,
               ", ",
               d11,
               ", ",
               d22,
               "). Expected primary diagonal ",
               _expected_diag_primary,
               " or neighbor diagonal ",
               _expected_diag_neighbor,
               ".");
  }
  _checked_rows = checked;
}

PostprocessorValue
KokkosInterfaceGatheringOracle::getValue() const
{
  return static_cast<PostprocessorValue>(_checked_rows);
}

#endif // NEML2_ENABLED
#endif // MOOSE_KOKKOS_ENABLED
