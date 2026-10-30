//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#ifdef MOOSE_KOKKOS_ENABLED
#ifdef NEML2_ENABLED

#include "GeneralUserObject.h"
#include "Postprocessor.h"
#include "KokkosInterfaceQuantityToNEML2.h"
#include "NEML2BatchIndexGenerator.h"

#include <array>

/**
 * Test-only postprocessor: after a KokkosInterfaceQuantityToNEML2 gather, verifies the
 * gathered NEML2 input buffer directly on the device it lives on. A Kokkos reduction over
 * the batch rows checks all nine row-major rank-two components of each row against the
 * expected primary- or neighbor-side pattern and counts the matching rows, so the buffer is
 * never dereferenced on the host. This makes the oracle valid for both CPU and CUDA Kokkos
 * without a host copy, host mirror, or .cpu() staging of the device buffer.
 * Returns the number of matched batch rows; a wrong value fails the CSV comparison.
 */
class KokkosInterfaceGatheringOracle : public GeneralUserObject, public Postprocessor
{
public:
  static InputParameters validParams();
  KokkosInterfaceGatheringOracle(const InputParameters & parameters);

  void initialize() override {}
  void execute() override;
  void finalize() override {}
  PostprocessorValue getValue() const override;

private:
  const KokkosInterfaceQuantityToNEML2 & _gatherer;
  const NEML2BatchIndexGenerator & _batch_index_generator;
  /// Expected full rank-two primary-side components, row-major (i * 3 + j).
  std::array<Real, 9> _expected_primary;
  /// Expected full rank-two neighbor-side components, row-major (i * 3 + j).
  std::array<Real, 9> _expected_neighbor;
  std::size_t _checked_rows = 0;
};

#endif // NEML2_ENABLED
#endif // MOOSE_KOKKOS_ENABLED
