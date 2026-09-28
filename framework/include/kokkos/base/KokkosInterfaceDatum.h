//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosDatum.h"
#include "KokkosInterfaceFaceCache.h"

namespace Moose::Kokkos
{

/**
 * Construct the datum of the primary side of an oriented interface face.
 *
 * An oriented face record already fixes both sides of the face, so the counterpart element of the
 * primary side is passed as the known neighbor instead of being resolved by a side topology
 * lookup. That lookup is only valid for active local elements, whereas the neighbor element of an
 * oriented interface face may be a one-layer ghost.
 *
 * Prefer this function over calling a Datum constructor with face fields directly. Only these two
 * functions know the primary/neighbor orientation of a face, so using them keeps a caller from
 * passing an inverted or omitted counterpart element.
 *
 * @tparam DatumType The datum type to construct, either Datum or AssemblyDatum
 * @tparam Args The trailing datum constructor arguments after the systems
 * @param face The oriented interface face record
 * @param assembly The Kokkos assembly
 * @param systems The Kokkos systems
 * @param args The trailing datum constructor arguments after the systems
 * @return The datum of the primary side of the face
 */
template <typename DatumType, typename... Args>
KOKKOS_FUNCTION DatumType
makePrimaryInterfaceDatum(const InterfaceFaceRecord & face,
                          const Assembly & assembly,
                          const Array<FESystem> & systems,
                          const Args &... args)
{
  return DatumType(face.elem, face.side, face.neighbor, assembly, systems, args...);
}

/**
 * Construct the datum of the neighbor side of an oriented interface face.
 *
 * This is the counterpart of makePrimaryInterfaceDatum(), and takes the same arguments. The
 * primary element is passed as the known neighbor, and the side is the one that the primary element
 * reports as adjacent to the neighbor element.
 *
 * @tparam DatumType The datum type to construct, either Datum or AssemblyDatum
 * @tparam Args The trailing datum constructor arguments after the systems
 * @param face The oriented interface face record
 * @param assembly The Kokkos assembly
 * @param systems The Kokkos systems
 * @param args The trailing datum constructor arguments after the systems
 * @return The datum of the neighbor side of the face
 */
template <typename DatumType, typename... Args>
KOKKOS_FUNCTION DatumType
makeNeighborInterfaceDatum(const InterfaceFaceRecord & face,
                           const Assembly & assembly,
                           const Array<FESystem> & systems,
                           const Args &... args)
{
  return DatumType(face.neighbor, face.neighbor_side, face.elem, assembly, systems, args...);
}

} // namespace Moose::Kokkos
