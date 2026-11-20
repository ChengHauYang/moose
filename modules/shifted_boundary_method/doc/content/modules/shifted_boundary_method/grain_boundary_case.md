# Grain Boundary Shifted Cohesive Zone Test

These tests solve a shifted cohesive zone (SCZM) problem on a polycrystalline grain structure. The
physics is identical across the cases; they differ only in how the grain-interface geometry is
supplied to the [`ShiftedCohesiveZoneAction`](ShiftedCohesiveZoneAction.md). Both two- and
three-dimensional variants are covered, and all variants produce the same solution.

## Three ways to supply the interface geometry

The action accepts the interface geometry in one of three forms. The distinction between the first
two and the third is whether the shared face between two grains must still be *found*:

1. **One complete boundary mesh.** A single saved mesh holds the complete, watertight boundary of
   every grain, each grain's surface tagged with its own subdomain ID. The action builds an
   [`SBMInterfaceManager`](SBMInterfaceManager.md), which detects each shared grain-grain interface
   by matching coincident faces between subdomain pairs. Use `complete_interface_mesh`.

2. **One watertight shell per grain.** Each grain's complete, watertight shell is a separate saved
   mesh. These still need the interface manager's finding step, because a shell is a closed boundary,
   not yet an interface. There are two equivalent ways to get the shells to the manager:

   - **2a, combine upstream.** Tag each shell with its grain ID using `SubdomainIDGenerator` and
     merge them into one mesh with `CombinerGenerator`, then feed that combined mesh to
     `complete_interface_mesh` (form 1). The combining is done in the input's `[Mesh]` block.

   - **2b, let the action combine.** Hand the per-grain shells straight to the action with
     `complete_interface_meshes` and the matching `interface_subdomain_ids`. The action stamps each
     shell with its grain ID and stitches them into the complete boundary mesh internally, replacing
     the `SubdomainIDGenerator` + `CombinerGenerator` boilerplate.

   Both paths build the same combined mesh, so the solution matches form 1 exactly.

3. **One mesh per grain interface.** Each saved mesh is already the shared face between two grains, so
   no interface manager is needed. The action builds one distance function per interface directly
   from its mesh. Use `generate_sbm_distance = true` with `surface_meshes`. The distance is computed
   by a different but equivalent path (an unsigned distance to the interface mesh with normal
   orientation recovered afterward), so the solution matches forms 1 and 2 to floating-point
   precision rather than being bit-for-bit identical.

In every case the action supplies the interface distances and normals to the traction model
(`BiLinearMixedModeTraction` in 2D, `PureElasticTractionSeparation` in 3D).

## Example inputs

| Form | Geometry supplied | Key parameters | Example input |
| - | - | - | - |
| 1. One complete boundary mesh | one mesh, all grain boundaries | `complete_interface_mesh` | `grain_boundary_sczm_coarse_lambda1.i` (2D), `grain_boundary_sczm_3d_one_file.i` (3D) |
| 2a. One shell per grain, combined upstream | one watertight shell per grain | `SubdomainIDGenerator` + `CombinerGenerator` -> `complete_interface_mesh` | `grain_boundary_sczm_separate_grains.i` (2D), `grain_boundary_sczm_3d_separate_grains.i` (3D) |
| 2b. One shell per grain, combined by the action | one watertight shell per grain | `complete_interface_meshes` + `interface_subdomain_ids` | `grain_boundary_sczm_separate_grains_list.i` (2D), `grain_boundary_sczm_3d_separate_grains_list.i` (3D) |
| 3. One mesh per interface | one shared face per grain pair | `generate_sbm_distance` + `surface_meshes` | `grain_boundary_sczm_separate_interfaces_aligned.i` (2D), `grain_boundary_sczm_3d_separate_interfaces.i` (3D) |

Form 1 drives the interface manager from the single combined mesh:

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_coarse_lambda1.i block=Physics/SolidMechanics/ShiftedCohesiveZone

Form 2a combines the per-grain shells in the `[Mesh]` block with `SubdomainIDGenerator` and
`CombinerGenerator`, then drives the manager from that combined mesh:

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_separate_grains.i block=Mesh

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_separate_grains.i block=Physics/SolidMechanics/ShiftedCohesiveZone

Form 2b hands the per-grain shells and their subdomain IDs straight to the action, which does the same
combining internally:

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_separate_grains_list.i block=Physics/SolidMechanics/ShiftedCohesiveZone

Form 3 supplies one already-formed interface mesh per surrogate boundary:

!listing modules/shifted_boundary_method/test/tests/grain_boundary_case/grain_boundary_sczm_separate_interfaces_aligned.i block=Physics/SolidMechanics/ShiftedCohesiveZone

## Three-dimensional meshes

The interface manager's surface element set accepts only `EDGE2` (2D) and `TRI3` (3D) surface
elements. A three-dimensional volume mesh feeding the manager (forms 1 and 2) must therefore be
`TET4`, whose boundary faces are `TRI3`; a hexahedral mesh errors because its faces are `QUAD4`.
