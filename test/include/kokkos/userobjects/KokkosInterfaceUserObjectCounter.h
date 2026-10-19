//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceUserObject.h"
#include "KokkosPostprocessor.h"

/** Counts primary interface quadrature points after exercising both interface data handles. */
class KokkosInterfaceUserObjectCounter : public Moose::Kokkos::InterfaceUserObject,
                                         public Moose::Kokkos::Postprocessor
{
public:
  static InputParameters validParams();

  KokkosInterfaceUserObjectCounter(const InputParameters & parameters);

  // Disambiguation with FunctorBase::operator()
  using InterfaceUserObject::operator();

  void initialize() override;
  void finalize() override;
  Real getValue() const override;

  template <typename Derived>
  KOKKOS_FUNCTION void executeInterface(Datum & primary,
                                        Datum & neighbor,
                                        const Moose::Kokkos::InterfaceFaceRecord & face) const;

private:
  Moose::Kokkos::Array<Real> _count;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosInterfaceUserObjectCounter::executeInterface(
    Datum & primary, Datum & neighbor, const Moose::Kokkos::InterfaceFaceRecord & face) const
{
  for (unsigned int qp = 0; qp < primary.n_qps(); ++qp)
  {
    const auto neighbor_qp = kokkosInterfaceNeighborQp(face, qp);
    const auto neighbor_n_qps = neighbor.n_qps();
    if (neighbor_qp >= neighbor_n_qps)
      ::Kokkos::abort("The mapped neighbor quadrature point is out of range.");

    ::Kokkos::atomic_add(&_count(0), 1.0);
  }
}
