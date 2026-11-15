//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosBatchIndexMapVerifier.h"

registerKokkosInterfaceUserObject("MooseTestApp", KokkosBatchIndexMapVerifier);

InputParameters
KokkosBatchIndexMapVerifier::validParams()
{
  InputParameters params = InterfaceUserObject::validParams();
  params += Moose::Kokkos::Postprocessor::validParams();
  params.addRequiredParam<UserObjectName>("batch_index_generator",
                                          "The NEML2 batch-index generator to verify.");
  params.addClassDescription("Verifies device-visible NEML2 interface batch-index maps.");
  return params;
}

KokkosBatchIndexMapVerifier::KokkosBatchIndexMapVerifier(const InputParameters & parameters)
  : InterfaceUserObject(parameters),
    Moose::Kokkos::Postprocessor(this),
    _batch_index_generator(getUserObject<NEML2BatchIndexGenerator>("batch_index_generator")),
    _primary_batch_indices(_batch_index_generator.kokkosFacePrimaryBatchIndices()),
    _neighbor_batch_indices(_batch_index_generator.kokkosFaceNeighborBatchIndices())
{
  _valid_faces.create(1);
}

void
KokkosBatchIndexMapVerifier::initialize()
{
  _valid_faces = 0;
}

void
KokkosBatchIndexMapVerifier::compute()
{
  if (_batch_index_generator.mapGeneration() == 0)
    mooseError(
        "The NEML2 batch-index generator did not publish a Kokkos mirror before verification.");

  _primary_batch_indices = _batch_index_generator.kokkosFacePrimaryBatchIndices();
  _neighbor_batch_indices = _batch_index_generator.kokkosFaceNeighborBatchIndices();
  InterfaceUserObject::compute();
}

void
KokkosBatchIndexMapVerifier::finalize()
{
  _valid_faces.copyToHost();
  gatherSum(_valid_faces(0));
}

Real
KokkosBatchIndexMapVerifier::getValue() const
{
  return _valid_faces(0);
}
