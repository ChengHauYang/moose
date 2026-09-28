//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosInterfaceFaceCache.h"

#include "KokkosAssembly.h"
#include "KokkosMesh.h"

#include "MooseObject.h"

#include <algorithm>

namespace Moose::Kokkos
{

InterfaceFaceCache::InterfaceFaceCache(const ::MooseMesh & mesh,
                                       const Mesh & kokkos_mesh,
                                       const Assembly & assembly,
                                       const std::set<BoundaryID> & boundary_ids,
                                       const MooseObject * error_object)
{
  std::vector<InterfaceFaceRecord> interface_faces;
  std::vector<unsigned int> neighbor_qp_map;
  std::set<Pair<ContiguousElementID, unsigned int>> visited_faces;
  const auto rank = mesh.getMesh().processor_id();

  for (const auto boundary : boundary_ids)
    for (const auto elem_id : mesh.getBoundaryActiveSemiLocalElemIds(boundary))
    {
      const auto * const elem = mesh.elemPtr(elem_id);
      if (elem->processor_id() != rank)
        continue;

      // An element can have several sides on the same interface boundary.
      for (const auto side : elem->side_index_range())
      {
        const auto boundary_ids_on_side = mesh.getBoundaryIDs(elem, side);
        if (std::find(boundary_ids_on_side.begin(), boundary_ids_on_side.end(), boundary) ==
            boundary_ids_on_side.end())
          continue;

        const Pair<ContiguousElementID, unsigned int> elem_side = {
            kokkos_mesh.getContiguousElementID(elem), side};
        if (!visited_faces.insert(elem_side).second)
          continue;

        const auto * const neighbor = elem->neighbor_ptr(side);
        if (!neighbor || neighbor == libMesh::remote_elem)
          error_object->paramError("boundary",
                                   "Kokkos interface faces must have a local or ghosted neighbor.");

        const auto neighbor_side = neighbor->which_neighbor_am_i(elem);
        if (neighbor_side == libMesh::invalid_uint)
          error_object->paramError("boundary",
                                   "Failed to identify the neighbor side of an interface face.");

        // Only one side of an interface face may carry the boundary, so that the cache holds a
        // single orientation of each physical face. Keeping both would assemble the interface
        // twice, and choosing one silently would decide which side is primary, which sets the
        // orientation of the face normal and the side the interface material reads.
        const auto neighbor_boundary_ids = mesh.getBoundaryIDs(neighbor, neighbor_side);
        if (std::find(neighbor_boundary_ids.begin(), neighbor_boundary_ids.end(), boundary) !=
            neighbor_boundary_ids.end())
          error_object->paramError(
              "boundary",
              "Kokkos interface boundary '",
              boundary,
              "' is set on both sides of the interface between element ",
              elem->id(),
              " and element ",
              neighbor->id(),
              ". Set it on only one side of each interface face.");

        const auto neighbor_id = kokkos_mesh.getContiguousElementID(neighbor);
        const auto elem_info = kokkos_mesh.getElementInfo(elem_side.first);
        const auto neighbor_info = kokkos_mesh.getElementInfo(neighbor_id);
        const auto n_qps = assembly.getNumFaceQps(elem_info, side);
        const auto n_neighbor_qps = assembly.getNumFaceQps(neighbor_info, neighbor_side);
        if (n_qps != n_neighbor_qps)
          error_object->paramError("boundary",
                                   "The two sides of a Kokkos interface face must have the same "
                                   "number of quadrature points.");

        const auto qp_map_offset = neighbor_qp_map.size();
        std::vector<bool> neighbor_qp_used(n_neighbor_qps, false);
        // Both points come from FE maps of the same face. This scale admits mapping roundoff while
        // remaining well below the spacing between distinct quadrature points.
        const Real matching_tol = 100 * TOLERANCE * elem->hmax();
        const Real matching_tol_sq = matching_tol * matching_tol;

        for (const auto qp : make_range(n_qps))
        {
          Real3 elem_point;
          assembly.computePhysicalMap(elem_info, side, qp, nullptr, nullptr, &elem_point, nullptr);

          unsigned int matched_qp = libMesh::invalid_uint;
          for (const auto neighbor_qp : make_range(n_neighbor_qps))
          {
            if (neighbor_qp_used[neighbor_qp])
              continue;

            Real3 neighbor_point;
            assembly.computePhysicalMap(neighbor_info,
                                        neighbor_side,
                                        neighbor_qp,
                                        nullptr,
                                        nullptr,
                                        &neighbor_point,
                                        nullptr);
            const auto distance = (elem_point - neighbor_point).norm();
            if (distance * distance <= matching_tol_sq)
            {
              if (matched_qp != libMesh::invalid_uint)
                error_object->paramError(
                    "boundary", "Kokkos interface quadrature point correspondence is ambiguous.");
              matched_qp = neighbor_qp;
            }
          }

          if (matched_qp == libMesh::invalid_uint)
            error_object->paramError(
                "boundary",
                "Failed to match a Kokkos interface quadrature point to the neighbor side.");

          neighbor_qp_used[matched_qp] = true;
          neighbor_qp_map.push_back(matched_qp);
        }

        interface_faces.push_back(
            {elem_side.first, elem_side.second, neighbor_id, neighbor_side, qp_map_offset, n_qps});
      }
    }

  _faces = interface_faces;
  _neighbor_qp_map = neighbor_qp_map;
}

} // namespace Moose::Kokkos
