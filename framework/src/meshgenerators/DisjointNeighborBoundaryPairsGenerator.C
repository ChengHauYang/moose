//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "DisjointNeighborBoundaryPairsGenerator.h"
#include "MooseMeshUtils.h"

#include "libmesh/boundary_info.h"
#include "libmesh/vector_value.h"

registerMooseObject("MooseApp", DisjointNeighborBoundaryPairsGenerator);

InputParameters
DisjointNeighborBoundaryPairsGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();

  params.addClassDescription(
      "Registers boundary pairs as disjoint neighbors on an input mesh. This is useful when the "
      "mesh topology and sidesets are read from a file format that does not preserve libMesh "
      "disjoint neighbor boundary pair metadata. If the input mesh is read from a file and will be "
      "run as a distributed mesh, the file reader should skip its initial partitioning so the "
      "disjoint neighbor pairs are available before the final mesh preparation.");

  params.addRequiredParam<MeshGeneratorName>("input", "The mesh on which to register pairs.");

  params.addRequiredParam<std::vector<std::vector<BoundaryName>>>(
      "boundary_pairs",
      "Boundary name/id pairs to register as disjoint neighbors. Each entry must contain exactly "
      "two boundaries.");

  params.addParam<std::vector<RealVectorValue>>("translations",
                                                {},
                                                "Optional translation vectors from the first "
                                                "boundary in each pair to the second. If omitted, "
                                                "zero translation is used for every pair.");

  return params;
}

DisjointNeighborBoundaryPairsGenerator::DisjointNeighborBoundaryPairsGenerator(
    const InputParameters & parameters)
  : MeshGenerator(parameters),
    _input(getMesh("input")),
    _boundary_pairs(getParam<std::vector<std::vector<BoundaryName>>>("boundary_pairs")),
    _translations(getParam<std::vector<RealVectorValue>>("translations"))
{
  if (_boundary_pairs.empty())
    paramError("boundary_pairs", "At least one boundary pair must be provided.");

  for (const auto i : index_range(_boundary_pairs))
    if (_boundary_pairs[i].size() != 2)
      paramError("boundary_pairs",
                 "Each boundary pair must contain exactly two boundaries, but pair ",
                 i,
                 " contains ",
                 _boundary_pairs[i].size(),
                 ".");

  if (!_translations.empty() && _translations.size() != _boundary_pairs.size())
    paramError("translations",
               "If provided, the number of translations must match the number of boundary pairs.");
}

std::unique_ptr<MeshBase>
DisjointNeighborBoundaryPairsGenerator::generate()
{
  std::unique_ptr<MeshBase> mesh = std::move(_input);

  const auto & boundary_info = mesh->get_boundary_info();
  auto boundary_ids = boundary_info.get_boundary_ids();
  mesh->comm().set_union(boundary_ids);

  for (const auto i : index_range(_boundary_pairs))
  {
    const auto boundary_id_1 = MooseMeshUtils::getBoundaryID(_boundary_pairs[i][0], *mesh);
    const auto boundary_id_2 = MooseMeshUtils::getBoundaryID(_boundary_pairs[i][1], *mesh);

    if (!boundary_ids.count(boundary_id_1))
      paramError("boundary_pairs",
                 "Boundary '",
                 _boundary_pairs[i][0],
                 "' in pair ",
                 i,
                 " does not exist on the input mesh.");
    if (!boundary_ids.count(boundary_id_2))
      paramError("boundary_pairs",
                 "Boundary '",
                 _boundary_pairs[i][1],
                 "' in pair ",
                 i,
                 " does not exist on the input mesh.");

    const auto translation =
        _translations.empty() ? RealVectorValue(0.0, 0.0, 0.0) : _translations[i];
    mesh->add_disjoint_neighbor_boundary_pairs(boundary_id_1, boundary_id_2, translation);
  }

  // The file reader may have already deleted remote elements, leaving the mesh non-serial while its
  // elements are still unpartitioned. In that state the final distributed prepare_for_use() would
  // try to partition unpartitioned elements on a non-serial mesh, which libMesh forbids. Restore
  // the serial state (a no-op on a ReplicatedMesh or an already-serial mesh) so the final
  // preparation can partition, re-run find_neighbors() with the disjoint neighbor pairs registered
  // above, and then redistribute.
  mesh->allgather();

  mesh->unset_is_prepared();

  return mesh;
}
