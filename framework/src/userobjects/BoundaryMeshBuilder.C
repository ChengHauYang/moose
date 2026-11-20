//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BoundaryMeshBuilder.h"
#include "MooseMesh.h"
#include "MeshGeneratorSystem.h"

#include "libmesh/unstructured_mesh.h"

registerMooseObject("MooseApp", BoundaryMeshBuilder);

InputParameters
BoundaryMeshBuilder::validParams()
{
  InputParameters params = GeneralUserObject::validParams();
  params.addClassDescription(
      "Owns a saved boundary mesh and the SurfaceElement wrappers built from it, "
      "for use by point-containment and distance user objects.");

  params.addParam<MeshGeneratorName>(
      "boundary_mesh",
      "The name of the boundary mesh saved via the MeshGenerator's `save_with_name` parameter. "
      "Mutually exclusive with 'boundary_meshes'.");
  params.addParam<std::vector<MeshGeneratorName>>(
      "boundary_meshes",
      "Names of per-subdomain boundary meshes saved via `save_with_name`, one per subdomain. Each "
      "is stamped with the subdomain ID in the parallel 'subdomain_ids' list and stitched into one "
      "boundary mesh. Mutually exclusive with 'boundary_mesh'.");
  params.addParam<std::vector<subdomain_id_type>>(
      "subdomain_ids",
      "Subdomain ID to stamp onto each mesh in 'boundary_meshes', in the same order.");

  params.addParam<bool>(
      "check_watertightness",
      false,
      "Check if the mesh is watertight. If false, the mesh may not be suitable for In-Out tests.");

  return params;
}

BoundaryMeshBuilder::BoundaryMeshBuilder(const InputParameters & parameters)
  : GeneralUserObject(parameters),
    // The list form takes precedence; the single-mesh name is only consumed when
    // the list is empty. getParam("boundary_mesh") resolves through the rename
    // alias when a subclass has renamed that parameter.
    _bnd_mesh_names(isParamValid("boundary_meshes")
                        ? getParam<std::vector<MeshGeneratorName>>("boundary_meshes")
                        : std::vector<MeshGeneratorName>()),
    _bnd_mesh_name(_bnd_mesh_names.empty() ? getParam<MeshGeneratorName>("boundary_mesh")
                                           : MeshGeneratorName()),
    _subdomain_ids(isParamValid("subdomain_ids")
                       ? getParam<std::vector<subdomain_id_type>>("subdomain_ids")
                       : std::vector<subdomain_id_type>()),
    _check_watertightness(getParam<bool>("check_watertightness")),
    _dim_embedding_mesh(_fe_problem.mesh().dimension() /*MooseMesh*/)
{
  const bool has_list = !_bnd_mesh_names.empty();
  if (has_list)
  {
    if (!isParamValid("subdomain_ids"))
      paramError("subdomain_ids", "'subdomain_ids' is required with 'boundary_meshes'.");
    if (_bnd_mesh_names.size() != _subdomain_ids.size())
      paramError("subdomain_ids",
                 "'subdomain_ids' must have one entry per mesh in 'boundary_meshes' (",
                 _bnd_mesh_names.size(),
                 " meshes, ",
                 _subdomain_ids.size(),
                 " ids).");
  }
  else if (isParamValid("subdomain_ids"))
    paramError("subdomain_ids", "'subdomain_ids' is only valid with 'boundary_meshes'.");
}

void
BoundaryMeshBuilder::initialSetup()
{
  // A saved mesh has single-retrieval semantics: getSavedMesh() std::move()s it
  // out and errors on a second retrieval. This builder is the sole owner.
  if (_bnd_mesh_names.empty())
    _mesh = _app.getMeshGeneratorSystem().getSavedMesh(_bnd_mesh_name);
  else
    _mesh = assembleFromPerSubdomainMeshes();

  // A saved mesh produced by a MeshGenerator may be unprepared, leaving
  // mesh_dimension() stale and element neighbor links unset. Prepare it so the
  // dimension check and the neighbor-based watertightness test are reliable.
  _mesh->prepare_for_use();

  if (!_mesh->is_replicated())
    mooseError("BoundaryMeshBuilder '",
               name(),
               "': the saved boundary mesh is distributed. A serialized/replicated boundary mesh "
               "is required for point-containment queries.");

  const auto expected_dim_embedding_mesh = _mesh->mesh_dimension() + 1;
  if (_dim_embedding_mesh != expected_dim_embedding_mesh)
    mooseError("BoundaryMeshBuilder '",
               name(),
               "': the background mesh dimension (",
               _dim_embedding_mesh,
               ") does not match the boundary mesh dimension + 1 (",
               expected_dim_embedding_mesh,
               ").");

  if (_check_watertightness)
  {
    if (checkWatertightness())
      mooseInfo("The mesh is watertight. It is suitable for In-Out tests.");
    else
      mooseInfo("The mesh is not watertight. It may not be suitable for In-Out tests.");
  }
}

std::unique_ptr<MeshBase>
BoundaryMeshBuilder::assembleFromPerSubdomainMeshes()
{
  auto & mesh_system = _app.getMeshGeneratorSystem();

  // Retrieve the first saved mesh as the accumulator and stamp its subdomain ID.
  // getSavedMesh() std::move()s each mesh out, so each name is consumed once.
  auto combined = mesh_system.getSavedMesh(_bnd_mesh_names[0]);
  auto * accumulator = dynamic_cast<UnstructuredMesh *>(combined.get());
  if (!accumulator)
    paramError("boundary_meshes", _bnd_mesh_names[0], " is not an unstructured mesh.");
  for (auto * elem : accumulator->element_ptr_range())
    elem->subdomain_id() = _subdomain_ids[0];

  // Stamp and stitch the remaining meshes into the accumulator. ID offsets keep
  // element/node IDs from colliding across the independently numbered meshes.
  for (const auto i : make_range(std::size_t(1), _bnd_mesh_names.size()))
  {
    auto other = mesh_system.getSavedMesh(_bnd_mesh_names[i]);
    auto * other_mesh = dynamic_cast<UnstructuredMesh *>(other.get());
    if (!other_mesh)
      paramError("boundary_meshes", _bnd_mesh_names[i], " is not an unstructured mesh.");
    for (auto * elem : other_mesh->element_ptr_range())
      elem->subdomain_id() = _subdomain_ids[i];

    accumulator->copy_nodes_and_elements(*other_mesh,
                                         /*skip_find_neighbors=*/false,
                                         accumulator->max_elem_id(),
                                         accumulator->max_node_id(),
                                         accumulator->parallel_max_unique_id());
  }

  return combined;
}

void
BoundaryMeshBuilder::buildDefaultSet() const
{
  _set = std::make_unique<SurfaceElementSet>(SurfaceElementSet::fromMesh(*_mesh));
}

bool
BoundaryMeshBuilder::checkWatertightness() const
{
  for (const auto * elem : _mesh->active_element_ptr_range())
    for (const auto s : make_range(elem->n_sides()))
      if (!elem->neighbor_ptr(s))
        return false;

  return true;
}

MeshBase &
BoundaryMeshBuilder::mesh() const
{
  mooseAssert(_mesh, "BoundaryMeshBuilder mesh is not available until initialSetup().");
  return *_mesh;
}

const SurfaceElementSet &
BoundaryMeshBuilder::surfaceElementSet() const
{
  // Build the whole-mesh set on first use so backends that never need it (e.g.
  // the fixed_x_ray TriangleManifold engine) pay no allocation for it.
  if (!_set)
    buildDefaultSet();

  if (!_set)
    mooseError("BoundaryMeshBuilder '", name(), "': buildDefaultSet() did not initialize _set.");
  return *_set;
}
