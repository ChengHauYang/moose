//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosAssembly.h"
#include "KokkosMesh.h"

#include "libmesh/elem.h"

#include <set>

class MooseObject;

namespace Moose::Kokkos
{

/** Topology and quadrature correspondence for one oriented interface face. */
struct InterfaceFaceRecord
{
  ContiguousElementID elem;
  unsigned int side;
  ContiguousElementID neighbor;
  unsigned int neighbor_side;
  dof_id_type qp_map_offset;
  unsigned int n_qps;
};

/**
 * The Kokkos cache of the oriented two-sided finite element interface faces of a boundary
 * restriction, together with the physical quadrature point correspondence across each face.
 *
 * FEProblemBase owns one cache per interface boundary restriction and builds it on first request.
 * Every interface material and interface kernel of the same boundary restriction handles a shallow
 * reference to the very same face arrays, so that a given local face index identifies the same face
 * for all of them. Only the rank owning the primary element of a face adds the face to its local
 * cache, which makes the primary element owner the single writer and updater of that face.
 */
class InterfaceFaceCache
{
public:
  /**
   * Constructor
   * @param mesh The MOOSE mesh
   * @param kokkos_mesh The Kokkos mesh
   * @param assembly The Kokkos assembly
   * @param boundary_ids The boundaries forming the interface
   * @param error_object The object used to report invalid interface topology
   */
  InterfaceFaceCache(const ::MooseMesh & mesh,
                     const Mesh & kokkos_mesh,
                     const Assembly & assembly,
                     const std::set<BoundaryID> & boundary_ids,
                     const MooseObject * error_object);

  /// Get the number of oriented interface faces cached on this rank
  dof_id_type n() const { return _faces.size(); }

  /**
   * Get an oriented interface face record
   * @param i The local face index
   */
  KOKKOS_FUNCTION const InterfaceFaceRecord & face(const dof_id_type i) const { return _faces[i]; }

  /**
   * Get the neighbor-side quadrature point index coinciding with a primary-side quadrature point
   * @param face The oriented interface face record
   * @param qp The primary-side local quadrature point index
   */
  KOKKOS_FUNCTION unsigned int neighborQp(const InterfaceFaceRecord & face,
                                          const unsigned int qp) const
  {
    return _neighbor_qp_map[face.qp_map_offset + qp];
  }

  /// Get the oriented interface face records
  const Array<InterfaceFaceRecord> & faces() const { return _faces; }

  /// Get the neighbor-side quadrature point index of each primary-side quadrature point
  const Array<unsigned int> & neighborQpMap() const { return _neighbor_qp_map; }

private:
  /// Oriented interface face records, one entry per cached face
  Array<InterfaceFaceRecord> _faces;
  /// Neighbor-side quadrature point index for each primary-side quadrature point
  Array<unsigned int> _neighbor_qp_map;
};

} // namespace Moose::Kokkos