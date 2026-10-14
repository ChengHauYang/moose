//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosTwoSidedAddressMaterial.h"

#include "FEProblemBase.h"

#include "libmesh/parallel.h"

#include <algorithm>

registerKokkosInterfaceMaterial("MooseTestApp", KokkosTwoSidedAddressMaterial);

InputParameters
KokkosTwoSidedAddressMaterial::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.makeParamRequired<std::vector<VariableName>>("variable");
  params.addRequiredParam<Real>("weight", "Weight of the primary-side variable in the address.");
  params.addClassDescription(
      "Writes the primary-side entry of the two-sided Kokkos property addressing fixture into "
      "boundary property storage at the primary face quadrature point.");
  return params;
}

KokkosTwoSidedAddressMaterial::KokkosTwoSidedAddressMaterial(const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _weight(getParam<Real>("weight")),
    _variable_value(kokkosCoupledValue("variable")),
    _side_address(declareKokkosProperty<Real>("side_address")),
    _dside_address_djump(declareKokkosProperty<Real>("dside_address_djump"))
{
}

registerKokkosInterfaceMaterial("MooseTestApp", KokkosTwoSidedAddressNeighborMaterial);

InputParameters
KokkosTwoSidedAddressNeighborMaterial::validParams()
{
  auto params = InterfaceMaterial::validParams();

  // Declares this material's properties into the neighbor property storage instead of the boundary
  // storage, which is what keeps the two sides of the fixture in two distinct arrays.
  params.set<Moose::MaterialDataType>("_material_data_type") = Moose::NEIGHBOR_MATERIAL_DATA;

  params.makeParamRequired<std::vector<VariableName>>("neighbor_var");
  params.addRequiredParam<Real>("weight", "Weight of the neighbor-side variable in the address.");
  params.addParam<bool>(
      "require_ghost_neighbor",
      true,
      "Require a parallel run to write at least one interface face whose neighbor element is a "
      "one-layer ghost. Only meaningful for a fixture whose purpose is to cover the ghost neighbor "
      "path; a mesh whose interface is not split across ranks cannot satisfy it.");
  params.addClassDescription(
      "Writes the neighbor-side entry of the two-sided Kokkos property addressing fixture into "
      "neighbor property storage at the neighbor face quadrature point.");
  return params;
}

KokkosTwoSidedAddressNeighborMaterial::KokkosTwoSidedAddressNeighborMaterial(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _weight(getParam<Real>("weight")),
    _neighbor_variable_value(kokkosCoupledValue("neighbor_var")),
    _neighbor_address(declareKokkosProperty<Real>("neighbor_address")),
    _dneighbor_address_djump(declareKokkosProperty<Real>("dneighbor_address_djump"))
{
  // Registered after the base class hook that fills the interface face cache, so the cache of this
  // boundary restriction already exists when this runs.
  _fe_problem.addKokkosMeshInitializationHook(
      std::bind(&KokkosTwoSidedAddressNeighborMaterial::checkGhostNeighborCoverage, this));
}

void
KokkosTwoSidedAddressNeighborMaterial::checkGhostNeighborCoverage()
{
  const auto * cache = _fe_problem.getKokkosInterfaceFaceCache(boundaryIDs(), this);
  const auto & ghost_elem_id_mapping = kokkosMesh().getGhostElemIdMapping();

  unsigned int n_ghost_neighbors = 0;
  for (const dof_id_type i : make_range(cache->n()))
  {
    const auto & face = cache->face(i);

    if (std::any_of(ghost_elem_id_mapping.cbegin(),
                    ghost_elem_id_mapping.cend(),
                    [&face](const auto & entry) { return entry.second == face.neighbor; }))
      ++n_ghost_neighbors;
  }

  // Reduce in place so that every rank knows the global count, not only those owning such a face.
  libmesh_call_mpi(MPI_Allreduce(
      MPI_IN_PLACE, &n_ghost_neighbors, 1, MPI_UNSIGNED, MPI_SUM, _mesh.comm().get()));

  if (_mesh.comm().size() > 1 && n_ghost_neighbors == 0 && getParam<bool>("require_ghost_neighbor"))
    mooseError(
        "No interface face of this material's boundary restriction was written with a "
        "one-layer ghost neighbor on any rank. This fixture covers writing the neighbor-side "
        "entry through a ghost element, so it needs a mesh whose interface is split across "
        "ranks.");
}
