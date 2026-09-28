//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosSideIntegralPostprocessor.h"

/**
 * Test postprocessor that samples and averages a specific component of the bulk stress
 * tensor across all quadrature points on an interface boundary.
 *
 * Uses unweighted quadrature point averaging so that evaluation on the coordinate
 * axis r = 0 in axisymmetric (RZ) coordinates does not divide by zero or encounter
 * a vanishing JxW metric weight.
 */
class KokkosSCZMBulkStressPostprocessor : public KokkosSideIntegralPostprocessor
{
public:
  static InputParameters validParams();

  KokkosSCZMBulkStressPostprocessor(const InputParameters & parameters);
  KokkosSCZMBulkStressPostprocessor(const KokkosSCZMBulkStressPostprocessor & object);

  template <typename Derived>
  KOKKOS_FUNCTION void reduce(Datum & datum, Real * result) const
  {
    Real sum = 0;
    Real count = 0;

    for (unsigned int qp = 0; qp < datum.n_qps(); ++qp)
    {
      sum += _stress(datum, qp)(_component_i, _component_j);
      count += 1.0;
    }

    result[0] += sum;
    result[1] += count;
  }

private:
  const unsigned int _component_i;
  const unsigned int _component_j;
  const Moose::Kokkos::MaterialProperty<Real, 2> _stress;
};
