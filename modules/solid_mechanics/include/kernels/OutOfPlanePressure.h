//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GenericKernel.h"

// Forward Declarations
class Function;

/**
 * OutOfPlanePressure is a kernel used to apply pressure in the out-of-plane direction
 * in 2D plane stress or generalized plane strain models. Following the convention of
 * the standard Pressure boundary condition, positive pressures are applied inward into
 * the surface.
 */

template <bool is_ad>
class OutOfPlanePressureTempl : public GenericKernel<is_ad>
{
public:
  static InputParameters validParams();

  OutOfPlanePressureTempl(const InputParameters & parameters);

protected:
  virtual GenericReal<is_ad> computeQpResidual() override;

private:
  const PostprocessorValue * const _postprocessor;
  const Function & _function;
  const Real _factor;

  usingGenericKernelMembers;
};

using OutOfPlanePressure = OutOfPlanePressureTempl<false>;
using ADOutOfPlanePressure = OutOfPlanePressureTempl<true>;
