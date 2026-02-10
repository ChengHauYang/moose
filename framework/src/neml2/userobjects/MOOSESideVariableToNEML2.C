//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MOOSESideVariableToNEML2.h"

registerMooseObject("MooseApp", MOOSESideVariableToNEML2);
registerMooseObject("MooseApp", MOOSEOldSideVariableToNEML2);

/**
 * This class is very similar to MOOSEVariableToNEML2, but it inherits from SideUserObject instead
 * of ElementUserObject, and thus gathers data on sidesets instead of elements.
 */
template <unsigned int state>
InputParameters
MOOSESideVariableToNEML2Templ<state>::validParams()
{
  auto params = MOOSEToNEML2SideBatched::validParams();
  params.addClassDescription("Gather a MOOSE variable on sidesets for insertion into the "
                             "specified input or model parameter of a NEML2 model.");
  params.addRequiredCoupledVar("from_moose", "MOOSE variable to read from");
  return params;
}

template <>
MOOSESideVariableToNEML2Templ<0>::MOOSESideVariableToNEML2Templ(const InputParameters & params)
  : MOOSEToNEML2SideBatched(params) // MOOSEToNEML2SideBatched is inherited from SideUserObject
#ifdef NEML2_ENABLED
    ,
    _moose_variable(coupledValue("from_moose"))
#endif
{
}

template <>
MOOSESideVariableToNEML2Templ<1>::MOOSESideVariableToNEML2Templ(const InputParameters & params)
  : MOOSEToNEML2SideBatched(params)
#ifdef NEML2_ENABLED
    ,
    _moose_variable(coupledValueOld("from_moose"))
#endif
{
}

template class MOOSESideVariableToNEML2Templ<0>;
template class MOOSESideVariableToNEML2Templ<1>;
