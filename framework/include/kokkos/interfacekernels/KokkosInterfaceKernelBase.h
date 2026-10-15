//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosResidualObject.h"
#include "KokkosLocalParallelInterface.h"
#include "KokkosInterfaceFaceCache.h"

#include "BoundaryRestrictableRequired.h"
#include "CoupleableMooseVariableDependencyIntermediateInterface.h"
#include "TwoMaterialPropertyInterface.h"
#include "MooseVariableFE.h"

#include "libmesh/enum_order.h"

namespace Moose::Kokkos
{

/** Base class for Kokkos two-sided finite element interface kernels. */
class InterfaceKernelBase : public ResidualObject,
                            public BoundaryRestrictableRequired,
                            public CoupleableMooseVariableDependencyIntermediateInterface,
                            public TwoMaterialPropertyInterface,
                            public LocalParallelInterface
{
public:
  static InputParameters validParams();

  InterfaceKernelBase(const InputParameters & parameters);
  InterfaceKernelBase(const InterfaceKernelBase & object);

  const MooseVariableFE<Real> & neighborVariable() const { return _neighbor_var; }

protected:
  KOKKOS_FUNCTION dof_id_type numKokkosInterfaceFaces() const { return _interface_faces.size(); }

  KOKKOS_FUNCTION const InterfaceFaceRecord & kokkosInterfaceFace(const ThreadID tid) const
  {
    return _interface_faces[tid];
  }

  KOKKOS_FUNCTION unsigned int kokkosInterfaceNeighborQp(const InterfaceFaceRecord & face,
                                                         const unsigned int qp) const
  {
    return _neighbor_qp_map[face.qp_map_offset + qp];
  }

  const Array<InterfaceFaceRecord> & kokkosInterfaceFaces() const { return _interface_faces; }

  /// Variable providing the neighbor-side rows and values.
  MooseVariableFE<Real> & _neighbor_var;
  /// Device-copyable metadata for the neighbor variable.
  Variable _kokkos_neighbor_var;

private:
  void acquireInterfaceFaces();

  /**
   * Shallow handles into the face arrays owned by the FEProblemBase interface face cache. They
   * refer to the same device memory as the interface materials of the same boundaries, so that a
   * local face index identifies the same face for all of them.
   */
  Array<InterfaceFaceRecord> _interface_faces;
  /// Neighbor-side quadrature point index for each primary-side quadrature point.
  Array<unsigned int> _neighbor_qp_map;
};

} // namespace Moose::Kokkos
