//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NEML2BatchIndexGenerator.h"

#include <algorithm>

#ifdef MOOSE_KOKKOS_ENABLED
#include "KokkosInterfaceFaceCache.h"
#include "KokkosMesh.h"
#endif

void
NEML2BatchIndexGenerator::updateKokkosMirrors()
{
#ifdef MOOSE_KOKKOS_ENABLED
  const auto invalid_batch_index = invalidKokkosBatchIndex();
  const auto * const kokkos_mesh = _fe_problem.mesh().getKokkosMesh();
  mooseAssert(kokkos_mesh, "Kokkos mesh must be initialized");

  // The Kokkos mesh is populated (contiguous element ids, local element count, element maps) only
  // once an actual Kokkos object triggers Moose::Kokkos::Mesh::update() (gated on hasKokkosObjects()
  // in MooseMesh::update()). These device mirrors are consumed only by the Kokkos NEML2 gatherers,
  // which require a Kokkos quantity and therefore always imply such an object, so the mesh is
  // guaranteed initialized whenever the mirrors are needed. With the Torch input backend this
  // generator still exists but no gatherer reads the mirrors and the mesh is never initialized;
  // building them anyway would read an unset extra-integer id out of bounds in
  // getContiguousElementID. Skip until the mesh is ready.
  if (!kokkos_mesh->initialized())
    return;

  if (_kokkos_element_batch_indices.size() != kokkos_mesh->getNumLocalElements())
    _kokkos_element_batch_indices.create(kokkos_mesh->getNumLocalElements());

  std::fill_n(_kokkos_element_batch_indices.hostData(),
              _kokkos_element_batch_indices.size(),
              invalid_batch_index);
  for (const auto elem : _mesh.getMesh().active_local_element_ptr_range())
  {
    const auto elem_id = kokkos_mesh->getContiguousElementID(elem);
    const auto it = _elem_to_batch_index.find(elem->id());
    if (it != _elem_to_batch_index.end())
      _kokkos_element_batch_indices.hostData()[elem_id] = it->second;
  }

  const auto n_faces = _interface_face_cache ? _interface_face_cache->n() : 0;
  if (_kokkos_face_primary_batch_indices.size() != n_faces)
  {
    _kokkos_face_primary_batch_indices.create(n_faces);
    _kokkos_face_neighbor_batch_indices.create(n_faces);
  }

  std::fill_n(_kokkos_face_primary_batch_indices.hostData(), n_faces, invalid_batch_index);
  std::fill_n(_kokkos_face_neighbor_batch_indices.hostData(), n_faces, invalid_batch_index);
  for (const auto face_index : make_range(n_faces))
  {
    const auto & face = _interface_face_cache->face(face_index);
    const auto primary = _elemside_to_batch_index.find(ElemSide(face.elem_id, face.side));
    if (primary != _elemside_to_batch_index.end())
      _kokkos_face_primary_batch_indices.hostData()[face.face_index] = primary->second;

    const auto neighbor =
        _elemside_to_batch_index.find(ElemSide(face.neighbor_id, face.neighbor_side));
    if (neighbor != _elemside_to_batch_index.end())
      _kokkos_face_neighbor_batch_indices.hostData()[face.face_index] = neighbor->second;
  }

  _kokkos_element_batch_indices.copyToDevice();
  _kokkos_face_primary_batch_indices.copyToDevice();
  _kokkos_face_neighbor_batch_indices.copyToDevice();
  ++_map_generation;
#endif
}
