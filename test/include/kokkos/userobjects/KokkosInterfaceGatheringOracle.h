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

/**
 * Test-only postprocessor: after a KokkosInterfaceQuantityToNEML2 gather, reads the
 * gatheredData() tensor (host memory on CPU Kokkos) and verifies that every diagonal
 * row matches either the expected primary- or neighbor-side diagonal value.
 * Returns the number of checked batch rows; a wrong value triggers mooseError.
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
  const Real _expected_diag_primary;
  const Real _expected_diag_neighbor;
  std::size_t _checked_rows = 0;
};

#endif // NEML2_ENABLED
#endif // MOOSE_KOKKOS_ENABLED
