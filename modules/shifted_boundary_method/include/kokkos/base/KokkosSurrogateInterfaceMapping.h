//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosArray.h"
#include "KokkosTypes.h"

#include "libmesh/id_types.h"

#include <vector>

namespace Moose::Kokkos
{

/// Device mapping shared by shifted interface objects using one boundary restriction and UO.
struct SurrogateInterfaceMapping
{
  struct HostFaceRecord
  {
    dof_id_type elem;
    unsigned short int side;
    dof_id_type qp_offset;
    unsigned int n_qps;
  };

  Array2D<dof_id_type> qp_offsets;
  Array<Real3> surrogate_distance_vectors;
  Array<Real3> true_normals;
  Array<Real33> rotations;
  std::vector<HostFaceRecord> host_faces;
  bool ready = false;
};

} // namespace Moose::Kokkos
