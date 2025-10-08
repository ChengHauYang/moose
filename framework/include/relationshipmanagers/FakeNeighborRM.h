#pragma once

#include "FunctorRelationshipManager.h"
#include <unordered_map>
#include "libmesh/elem.h"



namespace libMesh
{
class FakeNeighborFunctorImpl : public GhostingFunctor
{
public:
  explicit FakeNeighborFunctorImpl(
      const std::unordered_map<std::pair<const Elem *, unsigned int>,
                               std::pair<const Elem *, unsigned int>> & map)
    : _map(map), _dof_coupling(nullptr)
  {
  }

  void operator()(const MeshBase::const_element_iterator & range_begin,
                  const MeshBase::const_element_iterator & range_end,
                  processor_id_type p,
                  map_type & coupled_elements) override
  {
    for (const auto & elem : as_range(range_begin, range_end))
    {
      if (elem->processor_id() != p)
        coupled_elements.emplace(elem, nullptr);

      for (unsigned int side = 0; side < elem->n_sides(); ++side)
      {
        auto it = _map.find(std::make_pair(elem, side));
        if (it == _map.end())
          continue;

        const Elem * neigh = it->second.first;
        if (neigh && neigh->processor_id() != p)
          coupled_elements.emplace(neigh, _dof_coupling);
      }
    }
  }

  void set_dof_coupling(const libMesh::CouplingMatrix * dof_coupling) { _dof_coupling = dof_coupling; }

  bool map_empty() const { return _map.empty(); }

private:
  const std::unordered_map<std::pair<const Elem *, unsigned int>,
                           std::pair<const Elem *, unsigned int>> & _map;
  const libMesh::CouplingMatrix * _dof_coupling;
};
}

class FakeNeighborRM : public FunctorRelationshipManager
{
public:
  static InputParameters validParams();
  FakeNeighborRM(const InputParameters & params);

  // Required virtual functions from base classes
  std::string getInfo() const override;
  bool operator>=(const RelationshipManager & rhs) const override;
  std::unique_ptr<libMesh::GhostingFunctor> clone() const override;

  // Public method to set the map data
  void setFakeNeighborMap(
      const std::unordered_map<std::pair<const libMesh::Elem *, unsigned int>,
                               std::pair<const libMesh::Elem *, unsigned int>> & fake_neighbor_map);

protected:
  virtual void internalInitWithMesh(const MeshBase &) override;
  void dofmap_reinit() override;
  void set_mesh(const libMesh::MeshBase * mesh) override;

private:
  // The RM owns its own copy of the map.
  std::unordered_map<std::pair<const libMesh::Elem *, unsigned int>,
                     std::pair<const libMesh::Elem *, unsigned int>>
      _elem_side_to_fake_neighbor_elem_side;
};
