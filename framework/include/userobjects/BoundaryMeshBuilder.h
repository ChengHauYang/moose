//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GeneralUserObject.h"
#include "SurfaceElementSet.h"

#include "libmesh/mesh_base.h"

#include <memory>

/**
 * Owns a saved boundary mesh and the SurfaceElementSet wrapping it.
 *
 * This is the single object allowed to retrieve a saved mesh via
 * MeshGeneratorSystem::getSavedMesh(): that retrieval std::move()s the mesh out
 * of the mesh-generator system and hard-errors on a second retrieval, so exactly
 * one owner is required. Consumers (point-containment / distance user objects)
 * hold a non-owning reference to this builder instead of re-retrieving the mesh.
 *
 * The whole-mesh SurfaceElementSet is built by the virtual buildDefaultSet()
 * hook. Subclasses that need a different grouping (e.g. per subdomain) build
 * it under their own name instead of overriding this hook, so that
 * surfaceElementSet() still returns a correct whole-mesh set for any caller
 * that requests it via a BoundaryMeshBuilder reference.
 *
 * The boundary mesh may be supplied either as one saved mesh (`boundary_mesh`)
 * that already carries the per-subdomain grouping, or as a list of saved
 * per-subdomain meshes (`boundary_meshes`) each tagged with the subdomain ID in
 * the parallel `subdomain_ids` list. The list form stamps each mesh's elements
 * with its subdomain ID and stitches them into one owned mesh, which is
 * equivalent to combining the meshes with SubdomainIDGenerator + CombinerGenerator
 * upstream. A single boundary mesh may also be read directly from a file
 * (`boundary_mesh_file`), which works when the mesh generators do not run, e.g.
 * with a pre-split mesh (`--use-split`).
 */
class BoundaryMeshBuilder : public GeneralUserObject
{
public:
  static InputParameters validParams();
  BoundaryMeshBuilder(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void initialize() override {}
  virtual void execute() override {}
  virtual void finalize() override {}

  /// The owned boundary mesh. Valid after initialSetup(). Returned mutable
  /// because the TriangleManifold backend validates/improves the mesh in place;
  /// the builder itself does not modify it after initialSetup().
  MeshBase & mesh() const;

  /// The whole-mesh SurfaceElementSet, built lazily on first call. Must be called
  /// only from the single-threaded setup phase (e.g. initialSetup()): the lazy
  /// build is not re-entrant.
  const SurfaceElementSet & surfaceElementSet() const;

protected:
  /// Build the default (whole-mesh) SurfaceElementSet. Not intended to be
  /// overridden: subclasses that need a different grouping should build it
  /// under their own name and leave this hook alone, so that this whole-mesh
  /// set is still correctly built for any caller going through the base class.
  virtual void buildDefaultSet() const;

  /**
   * Whether the boundary mesh is "closed" -- i.e. has no open edges or faces.
   * On a manifold surface mesh this is equivalent to watertightness;
   * non-manifold input is not validated here.
   */
  bool checkWatertightness() const;

  /// The owned boundary mesh, kept alive so SurfaceElement pointers stay valid.
  std::unique_ptr<MeshBase> _mesh;

  /// The whole-mesh SurfaceElementSet, lazily cached on first use.
  mutable std::unique_ptr<SurfaceElementSet> _set;

  /// The current name of a parameter of this class that subclasses may have renamed (possibly
  /// more than once, e.g. boundary_mesh -> complete_boundary_mesh -> complete_interface_mesh)
  std::string renamedParam(std::string name) const;

  /// Retrieve the saved per-subdomain meshes named in `boundary_meshes`, stamp
  /// each with its subdomain ID from `subdomain_ids`, and stitch them into one
  /// owned mesh. Used in place of retrieving a single `boundary_mesh`.
  std::unique_ptr<MeshBase> assembleFromPerSubdomainMeshes();

  /// Names of saved per-subdomain meshes (list form), parallel to _subdomain_ids.
  /// Declared before _bnd_mesh_name so the latter can key off whether it is empty.
  const std::vector<MeshGeneratorName> _bnd_mesh_names;

  /// The name of a single mesh saved via a MeshGenerator's `save_with_name`
  /// parameter, or empty when the per-subdomain list form or a file is used instead.
  const MeshGeneratorName _bnd_mesh_name;

  /// A file holding the single boundary mesh, or empty when a saved mesh is used instead
  const FileName _bnd_mesh_file;

  /// Subdomain ID stamped onto each mesh in _bnd_mesh_names.
  const std::vector<subdomain_id_type> _subdomain_ids;

  /// Whether to check watertightness of the mesh (informational only).
  const bool _check_watertightness;

private:
  /// The dimension of the embedding (background) mesh.
  const unsigned int _dim_embedding_mesh;
};
