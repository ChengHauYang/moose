//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MOOSEToNEML2BatchedSide.h"

/**
 * Gather a MOOSE side variable for insertion into the NEML2 model.
 */
template <unsigned int state>
class MOOSESideVariableToNEML2Templ : public MOOSEToNEML2BatchedSide<Real>
{
public:
  static InputParameters validParams();

  MOOSESideVariableToNEML2Templ(const InputParameters & params);

#ifdef NEML2_ENABLED
protected:
  const MooseArray<Real> & sideMOOSEData() const override { return _moose_variable; }

  /// Coupled MOOSE variable to read data from
  const VariableValue & _moose_variable;
#endif
};

using MOOSESideVariableToNEML2 = MOOSESideVariableToNEML2Templ<0>;
using MOOSESideOldVariableToNEML2 = MOOSESideVariableToNEML2Templ<1>;
