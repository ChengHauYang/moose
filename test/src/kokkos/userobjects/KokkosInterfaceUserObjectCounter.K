//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosInterfaceUserObjectCounter.h"

registerKokkosInterfaceUserObject("MooseTestApp", KokkosInterfaceUserObjectCounter);

InputParameters
KokkosInterfaceUserObjectCounter::validParams()
{
  InputParameters params = InterfaceUserObject::validParams();
  params += Moose::Kokkos::Postprocessor::validParams();
  params.addClassDescription("Counts primary interface quadrature points while checking mapped "
                             "neighbor quadrature points.");
  return params;
}

KokkosInterfaceUserObjectCounter::KokkosInterfaceUserObjectCounter(
    const InputParameters & parameters)
  : InterfaceUserObject(parameters), Moose::Kokkos::Postprocessor(this)
{
  _count.create(1);
}

void
KokkosInterfaceUserObjectCounter::initialize()
{
  _count = 0;
}

void
KokkosInterfaceUserObjectCounter::finalize()
{
  _count.copyToHost();
  gatherSum(_count(0));
}

Real
KokkosInterfaceUserObjectCounter::getValue() const
{
  return _count(0);
}
