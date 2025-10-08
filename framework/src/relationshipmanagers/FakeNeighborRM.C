#include "FakeNeighborRM.h"
#include "MooseApp.h"
#include "FEProblemBase.h"
#include "libmesh/dof_map.h"
#include "libmesh/coupling_matrix.h"

using namespace libMesh;

registerMooseObject("MooseApp", FakeNeighborRM);

using namespace libMesh;

InputParameters
FakeNeighborRM::validParams()
{
  // No custom parameters needed, we set the data with a method call.
  return FunctorRelationshipManager::validParams();
}

FakeNeighborRM::FakeNeighborRM(const InputParameters & params) : FunctorRelationshipManager(params)
{
}

void
FakeNeighborRM::setFakeNeighborMap(
    const std::unordered_map<std::pair<const libMesh::Elem *, unsigned int>,
                             std::pair<const libMesh::Elem *, unsigned int>> & fake_neighbor_map)
{
  // Make our own safe copy of the map data.
  _elem_side_to_fake_neighbor_elem_side = fake_neighbor_map;
}

void
FakeNeighborRM::internalInitWithMesh(const MeshBase &)
{
  // Create the functor, passing a const reference to our OWN copy of the map.
  if (!_functor)
    _functor = std::make_unique<FakeNeighborFunctorImpl>(_elem_side_to_fake_neighbor_elem_side);
  else if (auto fn = dynamic_cast<FakeNeighborFunctorImpl *>(_functor.get()))
  {
    if (fn->map_empty() && !_elem_side_to_fake_neighbor_elem_side.empty())
      _functor = std::make_unique<FakeNeighborFunctorImpl>(_elem_side_to_fake_neighbor_elem_side);
  }
}

void
FakeNeighborRM::dofmap_reinit()
{
  if (_dof_map)
    static_cast<FakeNeighborFunctorImpl *>(_functor.get())
        ->set_dof_coupling(_dof_map->_dof_coupling);
}

// Implementation of required pure virtual functions
std::string
FakeNeighborRM::getInfo() const
{
  return "FakeNeighborRM (ghosts across artificial neighbors)";
}

bool
FakeNeighborRM::operator>=(const RelationshipManager & rhs) const
{
  const auto * rm = dynamic_cast<const FakeNeighborRM *>(&rhs);
  if (!rm)
    return false;
  return baseGreaterEqual(*rm);
}

std::unique_ptr<GhostingFunctor>
FakeNeighborRM::clone() const
{
  return _app.getFactory().copyConstruct(*this);
}

void
FakeNeighborRM::set_mesh(const libMesh::MeshBase * mesh)
{
  // 1. Just-in-time creation
  if (!_functor)
    _functor = std::make_unique<FakeNeighborFunctorImpl>(_elem_side_to_fake_neighbor_elem_side);

  // 2. Now that we guarantee _functor exists, safely call the base class version.
  FunctorRelationshipManager::set_mesh(mesh);
}
