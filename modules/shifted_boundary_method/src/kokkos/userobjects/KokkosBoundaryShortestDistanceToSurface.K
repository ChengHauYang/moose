//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BoundaryShortestDistanceToSurface.h"

#include "CohesiveZoneModelTools.h"
#include "KokkosInterfaceFaceCache.h"
#include "KokkosMesh.h"

const Moose::Kokkos::SurrogateInterfaceMapping &
BoundaryShortestDistanceToSurface::registerKokkosSurrogateInterfaceMapping(
    const std::set<BoundaryID> & boundary_ids,
    const Moose::Kokkos::Array<Moose::Kokkos::InterfaceFaceRecord> & faces) const
{
  if (!hasBoundary(boundary_ids))
    mooseError("The Kokkos shifted interface boundary restriction is not contained in the '",
               name(),
               "' BoundaryShortestDistanceToSurface boundary restriction.");

  auto & mapping = _kokkos_surrogate_interface_mappings[boundary_ids];
  if (mapping)
    return *mapping;

  mapping = std::make_shared<Moose::Kokkos::SurrogateInterfaceMapping>();

  dof_id_type n_qps = 0;
  for (const auto & face : faces)
    n_qps += face.n_qps;

  const auto & mesh = *_mesh.getKokkosMesh();
  mapping->qp_offsets.create(_mesh.getMaxSidesPerElem(),
                             mesh.getNumLocalAndPossiblyOneNeighborLayerGhostElements());
  mapping->qp_offsets = libMesh::DofObject::invalid_id;
  mapping->surrogate_distance_vectors.create(n_qps);
  mapping->true_normals.create(n_qps);
  mapping->rotations.create(n_qps);
  mapping->host_faces.reserve(faces.size());

  std::map<ContiguousElementID, const Elem *> host_elements;
  for (const auto boundary : boundary_ids)
    for (const auto elem_id : _mesh.getBoundaryActiveSemiLocalElemIds(boundary))
    {
      const auto * const elem = _mesh.elemPtr(elem_id);
      if (elem->processor_id() == processor_id())
        host_elements.emplace(mesh.getContiguousElementID(elem), elem);
    }

  for (const auto & face : faces)
  {
    const auto * const elem = libmesh_map_find(host_elements, face.elem);
    mapping->qp_offsets(face.side, face.elem) = face.qp_map_offset;
    mapping->host_faces.push_back(
        {elem->id(), static_cast<unsigned short int>(face.side), face.qp_map_offset, face.n_qps});
  }
  mapping->qp_offsets.copyToDevice();

  return *mapping;
}

bool
BoundaryShortestDistanceToSurface::kokkosSurrogateInterfaceMappingReady(
    const std::set<BoundaryID> & boundary_ids) const
{
  const auto it = _kokkos_surrogate_interface_mappings.find(boundary_ids);
  return it != _kokkos_surrogate_interface_mappings.end() && it->second->ready;
}

void
BoundaryShortestDistanceToSurface::populateSurrogateInterfaceMappings()
{
  for (auto & [_, mapping] : _kokkos_surrogate_interface_mappings)
  {
    for (const auto & face : mapping->host_faces)
    {
      const ElemSide elem_side(face.elem, face.side);
      for (const auto qp : make_range(face.n_qps))
      {
        const auto index = face.qp_offset + qp;
        mapping->surrogate_distance_vectors[index] = surrogateDistance(elem_side, qp);
        mapping->true_normals[index] = trueNormal(elem_side, qp);
        mapping->rotations[index] = CohesiveZoneModelTools::computeReferenceRotation(
            trueNormal(elem_side, qp), _mesh.dimension());
      }
    }
    mapping->surrogate_distance_vectors.copyToDevice();
    mapping->true_normals.copyToDevice();
    mapping->rotations.copyToDevice();
    mapping->ready = true;
  }
}
