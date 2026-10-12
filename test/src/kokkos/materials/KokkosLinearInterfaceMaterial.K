//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosLinearInterfaceMaterial.h"

#include "FEProblemBase.h"

#include "libmesh/int_range.h"
#include "libmesh/parallel.h"

#include <algorithm>

registerKokkosInterfaceMaterial("MooseTestApp", KokkosLinearInterfaceMaterial);

InputParameters
KokkosLinearInterfaceMaterial::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.makeParamRequired<std::vector<VariableName>>("variable");
  params.makeParamRequired<std::vector<VariableName>>("neighbor_var");
  params.addRequiredParam<Real>("stiffness", "Traction stiffness of the interface.");
  params.addParam<unsigned int>("expected_interface_faces",
                                "Expected global number of oriented interface faces.");
  params.addParam<bool>(
      "require_ghost_neighbor",
      true,
      "Require a parallel run to evaluate at least one interface face whose neighbor element is a "
      "one-layer ghost. Only meaningful for a test whose purpose is to cover the ghost path; a "
      "mesh "
      "whose interface is not split across ranks cannot satisfy it.");
  return params;
}

KokkosLinearInterfaceMaterial::KokkosLinearInterfaceMaterial(const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _stiffness(getParam<Real>("stiffness")),
    _traction(declareKokkosPropertyByName<Real>("traction")),
    _dtraction_djump(declareKokkosPropertyByName<Real>("dtraction_djump"))
{
  // Registered after the base class hook that fills the interface face cache, so the cache of this
  // boundary restriction already exists when this runs.
  _fe_problem.addKokkosMeshInitializationHook(
      std::bind(&KokkosLinearInterfaceMaterial::checkGhostNeighborCoverage, this));
}

void
KokkosLinearInterfaceMaterial::checkGhostNeighborCoverage()
{
  const auto * cache = _fe_problem.getKokkosInterfaceFaceCache(boundaryIDs(), this);

  if (isParamValid("expected_interface_faces"))
  {
    auto n_faces = cache->n();
    _mesh.comm().sum(n_faces);
    if (n_faces != getParam<unsigned int>("expected_interface_faces"))
      mooseError("Expected ",
                 getParam<unsigned int>("expected_interface_faces"),
                 " interface faces, but cached ",
                 n_faces,
                 ".");
  }

  const auto & ghost_elem_id_mapping = kokkosMesh().getGhostElemIdMapping();

  unsigned int n_ghost_neighbors = 0;
  for (const dof_id_type i : make_range(cache->n()))
  {
    const auto & face = cache->face(i);

    // The neighbor element has to resolve to Kokkos element data, which it does whether it is
    // locally owned or a one-layer ghost. Reading it through the side topology table instead
    // returns uninitialized memory for a ghost, because that table only covers active local
    // elements.
    if (face.neighbor >= kokkosMesh().getNumLocalAndPossiblyOneNeighborLayerGhostElements())
      mooseError("Interface face with primary element ",
                 face.elem,
                 " side ",
                 face.side,
                 " refers to neighbor element ",
                 face.neighbor,
                 ", which is outside the local and one-layer ghost element range of this rank.");

    if (std::any_of(ghost_elem_id_mapping.cbegin(),
                    ghost_elem_id_mapping.cend(),
                    [&face](const auto & entry) { return entry.second == face.neighbor; }))
      ++n_ghost_neighbors;
  }

  // Reduce in place so that the global count of interface faces with a ghost neighbor is known on
  // every rank, not only on those owning such a face.
  libmesh_call_mpi(MPI_Allreduce(
      MPI_IN_PLACE, &n_ghost_neighbors, 1, MPI_UNSIGNED, MPI_SUM, _mesh.comm().get()));

  // Fail loudly instead of silently losing the coverage this test exists to provide, which a
  // repartitioning that keeps the interface faces and their neighbors together would cause.
  if (_mesh.comm().size() > 1 && n_ghost_neighbors == 0 && getParam<bool>("require_ghost_neighbor"))
    mooseError("No interface face of this material's boundary restriction was evaluated with a "
               "one-layer ghost neighbor on any rank. This test covers the neighbor-side "
               "interface datum of a ghost element, so it needs a mesh whose interface is split "
               "across ranks.");
}
