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
#include "NEML2BatchIndexGenerator.h"

/** Verifies device-visible primary and neighbor NEML2 interface batch indices. */
class KokkosBatchIndexMapVerifier : public Moose::Kokkos::InterfaceUserObject,
                                    public Moose::Kokkos::Postprocessor
{
public:
  static InputParameters validParams();

  KokkosBatchIndexMapVerifier(const InputParameters & parameters);

  using InterfaceUserObject::operator();

  void initialize() override;
  void compute() override;
  void finalize() override;
  Real getValue() const override;

  template <typename Derived>
  KOKKOS_FUNCTION void executeInterface(Datum & primary,
                                        Datum & neighbor,
                                        const Moose::Kokkos::InterfaceFaceRecord & face) const;

private:
  const NEML2BatchIndexGenerator & _batch_index_generator;
  Moose::Kokkos::Array<std::size_t> _primary_batch_indices;
  Moose::Kokkos::Array<std::size_t> _neighbor_batch_indices;
  Moose::Kokkos::Array<Real> _valid_faces;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosBatchIndexMapVerifier::executeInterface(Datum & /* primary */,
                                              Datum & /* neighbor */,
                                              const Moose::Kokkos::InterfaceFaceRecord & face) const
{
  const auto invalid_batch_index = NEML2BatchIndexGenerator::invalidKokkosBatchIndex();
  if (_primary_batch_indices(face.face_index) == invalid_batch_index ||
      _neighbor_batch_indices(face.face_index) == invalid_batch_index)
    ::Kokkos::abort("The NEML2 interface batch-index mirror has an invalid face entry.");

  ::Kokkos::atomic_add(&_valid_faces(0), 1.0);
}
