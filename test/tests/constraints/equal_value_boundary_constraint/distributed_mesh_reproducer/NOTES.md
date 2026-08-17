# EqualValueBoundaryConstraint on a DistributedMesh

This is both a tutorial and an investigation log for the crash reproduced by
the inputs in this directory. Start with the tutorial below. The later sections
preserve the original investigation and may contain superseded hypotheses;
claims there are tagged **[verified]** or **[unverified]**.

## Tutorial: why one element needs two kinds of ghosting

### The short version

`EqualValueBoundaryConstraint` (EVBC) manually gathers the element connected to
its primary node. A later `delete_remote_elements()` may delete that element
again unless EVBC tells `DistributedMesh` to retain it. EVBC also needs the
element's DoFs in the algebraic send list. These are separate requirements:

```cpp
Elem * const primary_elem = _mesh.elemPtr(elems[0]);

// Geometric retention: keep the Elem object through remote-element deletion.
if (auto * const distributed_mesh =
        dynamic_cast<DistributedMesh *>(&_mesh.getMesh()))
  distributed_mesh->add_extra_ghost_elem(primary_elem);

// Algebraic ghosting: make the element's DoFs available on this processor.
_subproblem.addGhostedElem(primary_elem->id());
```

Neither call replaces the other.

### 1. Geometric ghosting and algebraic ghosting solve different problems

A distributed mesh does not keep every element on every processor. There are
two related but distinct questions:

1. Is the actual `Elem` object present on this processor?
2. Are the element's DoF values included in the processor's algebraic ghosted
   vectors and send list?

`DistributedMesh::add_extra_ghost_elem(elem)` answers the first question. It
registers an element that must survive `delete_remote_elements()`. The libMesh
API describes this as retaining an off-processor element and its descendants.
This is geometric mesh retention.

`SubProblem::addGhostedElem(elem_id)` answers the second question. It stores the
ID in `SubProblem::_ghosted_elems`; `SystemBase::augmentSendList()` later finds
the element and adds its DoF indices to the send list. This is algebraic DoF
ghosting. Historically, this API stores an ID but does not itself state why or
for how long the mesh must retain the corresponding `Elem` object.

A useful mental model is:

| Operation | Keeps `Elem` alive? | Ghosts its DoFs? |
| --- | --- | --- |
| `add_extra_ghost_elem(elem)` | yes | no |
| `addGhostedElem(elem->id())` | not by itself | yes |

#### Ghosting reference

"Ghosting" means making non-owned data available on a processor, but the word
is overloaded. The remote mesh object, its DoF values, and its matrix coupling
are separate concerns.

**Geometric ghosting** controls which remote `Elem` and `Node` objects remain in
a processor's `MeshBase`. It supports operations that need connectivity, sides,
coordinates, subdomain IDs, ownership, or neighbor traversal. A geometric
RelationshipManager expresses a persistent mesh rule, for example:

```cpp
params.addRelationshipManager(
    "ElementSideNeighborLayers",
    Moose::RelationshipManagerType::GEOMETRIC,
    [](const InputParameters &, InputParameters & rm_params)
    { rm_params.set<unsigned short>("layers") = 1; });
```

This retains one neighboring layer when `delete_remote_elements()` removes
unneeded off-processor elements. It does not by itself guarantee that current
solution values for every DoF on those elements are locally readable.

**Algebraic ghosting** controls which non-owned DoF values are represented in a
processor's ghosted vectors. After vector synchronization, local code can read
those values without communicating for each access. In the EVBC path,
`addGhostedElem()` eventually makes `SystemBase::augmentSendList()` look up the
`Elem`, find its DoF indices, and add them to the send list. Despite its name,
`addGhostedElem()` is an algebraic request; it does not promise that the `Elem`
will survive geometric cleanup.

**Coupling ghosting** describes which remote DoFs may interact with local
residual equations and Jacobian entries. MOOSE uses this information to build
matrix sparsity and communication structures. The RelationshipManager roles
answer different questions:

| RelationshipManager type | Question answered |
| --- | --- |
| `GEOMETRIC` | Which remote mesh entities must exist locally? |
| `ALGEBRAIC` | Which remote DoF values must be readable locally? |
| `COUPLING` | Which remote DoFs may participate in matrix couplings? |

An object can request multiple roles by combining their flags. For example, an
algorithm that traverses a neighboring element and evaluates a variable there
may require both `GEOMETRIC` and `ALGEBRAIC` ghosting.

**Explicit extra ghost elements** are a direct geometric-retention mechanism:

```cpp
distributed_mesh->add_extra_ghost_elem(elem);
```

This is appropriate when code manually obtains a particular remote `Elem` that
is not covered by a normal geometric RelationshipManager and needs that object
after cleanup. It does not ghost the element's DoFs, so EVBC needs both calls:

```cpp
distributed_mesh->add_extra_ghost_elem(primary_elem); // Retain the Elem.
_subproblem.addGhostedElem(primary_elem->id());       // Ghost its DoFs.
```

**Semilocal elements** are locally owned elements plus remote ghost elements
that are currently present on the processor. Iterating over
`semilocal_elements_begin()` does not fetch new remote elements; it only visits
objects already available. Calling `addGhostedElem()` for those IDs requests
algebraic data but does not create or geometrically retain a new mesh halo.

**Metadata synchronization is not ghosting.** If an algorithm only needs a
small result computed from a remote element, the owner can communicate element
IDs, boundary IDs, connectivity, matrices, vectors, or flags instead of
retaining the entire `Elem`. BMBB synchronizes topology metadata, and
`NodalPatchRecoveryBase` synchronizes `_Ae` and `_be` patch contributions.

The stencil distance is not the deciding factor. "One element away" can mean:

| Requirement | Appropriate mechanism |
| --- | --- |
| Inspect a neighboring element's geometry | Geometric RelationshipManager |
| Read neighboring variable values | Algebraic RelationshipManager |
| Assemble couplings to neighboring DoFs | Coupling RelationshipManager |
| Keep a manually obtained remote `Elem` after cleanup | `add_extra_ghost_elem()` |
| Use a compact result computed on a remote owner | Synchronize the metadata/result |

Use these questions when selecting a mechanism:

1. Do I need the actual remote `Elem`, or only data computed from it?
2. If I need the `Elem`, does a geometric RelationshipManager already claim it?
3. If not, was it manually obtained and must it survive cleanup or repartition?
4. Do I separately need its variable or DoF values?
5. Can its DoFs couple into the residual or Jacobian?

The answers are independent. Needing geometry does not imply algebraic data,
needing DoF values does not automatically retain geometry, and neither implies
that an arbitrary manually gathered element will survive deletion.

### 2. Why EVBC needs both

`EqualValueBoundaryConstraint::ghostPrimary()` does the following on a
distributed mesh:

1. Finds elements connected to the primary node on processors that have them.
2. Uses `allgather_packed_range()` to inject their nodes and elements into every
   processor's mesh.
3. Calls `_mesh.update()` to rebuild connectivity.
4. Records one connected element with `_subproblem.addGhostedElem(elems[0])`.

The gather only makes the object present at that moment. It does not register a
persistent ghosting rule or mark the gathered element as protected. If a late
geometric relationship manager causes `delete_remote_elements()` after EVBC is
constructed, libMesh is free to remove that off-processor element.

The remaining `_ghosted_elems` entry is then a stale ID. During system
initialization, `SystemBase::augmentSendList()` calls `_mesh.elemPtr(elem_id)`.
The debug build asserts in `distributed_mesh.C`; an optimized build may instead
dereference null.

The required invariant is therefore:

> Every ID in `SubProblem::_ghosted_elems` must still identify a present `Elem`
> when `SystemBase::augmentSendList()` consumes it.

For EVBC, `add_extra_ghost_elem()` preserves the object and `addGhostedElem()`
preserves the required algebraic data.

### 3. `SampledOutput` is the closest existing precedent

`framework/src/outputs/SampledOutput.C` uses the same libMesh API:

```cpp
// If we're going to be copying from that system later, we need to keep its
// original elements as ghost elements even if it gets grossly repartitioned.
DistributedMesh * dist_mesh =
    dynamic_cast<DistributedMesh *>(&source_es.get_mesh());
if (dist_mesh)
  for (auto & elem : dist_mesh->active_local_element_ptr_range())
    dist_mesh->add_extra_ghost_elem(elem);
```

The key similarity is ownership of intent:

- `SampledOutput` knows it will need those source elements after repartitioning,
  so `SampledOutput` requests their retention.
- EVBC knows it manually gathered a primary element and will later use its ID,
  so EVBC should request that element's retention.

This precedent supports using `add_extra_ghost_elem()` for the failure mode. It
also supports putting the call in the component that knows why the element must
survive.

### 4. Why BMBB does not need `add_extra_ghost_elem()`

Using a distributed mesh does not by itself require every component to call
`add_extra_ghost_elem()`. That API is needed when code manually obtains a remote
element outside the normal geometric ghosting rules and still needs the element
after remote-element cleanup.

`BreakMeshByBlockGenerator` (BMBB) declares the geometric dependency it needs in
`validParams()`:

```cpp
params.addRelationshipManager(
    "ElementSideNeighborLayers",
    Moose::RelationshipManagerType::GEOMETRIC,
    [](const InputParameters &, InputParameters & rm_params)
    { rm_params.set<unsigned short>("layers") = 1; });
```

This tells MOOSE/libMesh to provide and retain one side-neighbor layer while
BMBB operates. BMBB therefore uses remote neighbor elements supplied by a
normal `RelationshipManager`; it does not manually gather an otherwise
unclaimed remote element.

BMBB also does not leave an element ID for a later system stage to consume. Its
element pointers and local node-to-element map are temporary data used within
one `generate()` call:

```text
generate()
  -> inspect current local and ghost elements
  -> duplicate interface nodes
  -> record interface sides
  -> addInterface()
  -> return the modified mesh
```

The completed topology and boundary information become part of the returned
mesh. BMBB can then let normal distributed-mesh cleanup discard remote elements
that no longer satisfy a ghosting rule.

When BMBB needs information from other processors, it communicates the metadata
rather than retaining all remote elements. Examples include:

- `syncConnectedBlocks()` for node-to-block connectivity.
- `mesh.comm().set_union(_neighboring_block_list)` for global block pairs.
- `parallel_sync_side_ids()` and `parallel_sync_node_ids()` for boundary data.

EVBC has a different lifetime:

```text
EVBC::ghostPrimary()
  -> manually allgather a primary element
  -> store its ID in SubProblem::_ghosted_elems
  -> leave constraint setup
  -> later SystemBase::augmentSendList() looks up that ID
```

No geometric `RelationshipManager` claims that manually gathered primary
element. A late `delete_remote_elements()` can therefore remove it between the
producer and consumer unless EVBC calls `add_extra_ghost_elem()`.

The distinction is:

| Question | BMBB | EVBC |
| --- | --- | --- |
| How are remote elements obtained? | `ElementSideNeighborLayers` RM | Manual `allgather_packed_range()` |
| Does libMesh know the retention rule? | Yes | Not without `add_extra_ghost_elem()` |
| Is an element ID consumed in a later stage? | No | Yes |
| Is element data replaced by synchronized metadata? | Yes, where possible | No, the later code needs the `Elem` and its DoFs |
| Needs explicit extra retention? | No | Yes |

A useful classification is:

```text
local element                  -> retained by mesh ownership
normal geometric ghost         -> retained by RM/GhostingFunctor
manually obtained remote Elem  -> use add_extra_ghost_elem() if needed later
```

If BMBB were changed to manually gather an element outside its RM coverage and
save that element or its ID for use after `generate()`, then BMBB would also
need explicit retention or a more suitable RelationshipManager.

#### `NodalPatchRecovery` is another non-example

Needing an element one layer away does not by itself imply a need for
`add_extra_ghost_elem()`. Distance describes the stencil; explicit retention is
about how the remote `Elem` entered the mesh and whether it must survive a later
geometric cleanup.

There are two similarly named patch-recovery implementations to distinguish.
The legacy `NodalPatchRecovery` AuxKernel does this in its constructor:

```cpp
for (const auto & elem :
     as_range(meshhelper.semilocal_elements_begin(), meshhelper.semilocal_elements_end()))
  _fe_problem.addGhostedElem(elem->id());
```

This loop does not discover or insert new remote elements. It only records IDs
for elements that the mesh already classifies as semilocal, so that their DoFs
are added to the algebraic send list. It therefore does not need to call
`add_extra_ghost_elem()` for those elements. This implementation also explicitly
rejects parallel nodal execution. Its commented-out RelationshipManager and
manual ghosting are legacy limitations, not evidence that `addGhostedElem()`
provides geometric retention.

The parallel-capable `NodalPatchRecoveryBase` UserObject declares a different
requirement:

```cpp
params.addRelationshipManager("ElementSideNeighborLayers",
                              Moose::RelationshipManagerType::ALGEBRAIC,
                              [](const InputParameters &, InputParameters & rm_params)
                              {
                                rm_params.set<bool>("use_point_neighbors") = true;
                                rm_params.set<unsigned short>("layers") = 1;
                              });
```

The point-neighbor option is important for partition corners: an element in the
nodal patch may share only a node, not a face, with a locally owned element. The
one-layer relationship supplies the algebraic patch stencil. The UserObject
runs on owned elements to compute per-element `_Ae` and `_be`, then `finalize()`
uses `pull_parallel_vector_data()` to synchronize those small matrices and
vectors for remote patch elements. It communicates the recovery data rather
than manually allgathering and retaining arbitrary `Elem` objects.

The comparison is:

| Case | Why the remote data is needed | Mechanism | Extra geometric retention? |
| --- | --- | --- | --- |
| Legacy `NodalPatchRecovery` | DoFs on already-semilocal elements | `addGhostedElem()`; nodal mode is serial-only | No |
| `NodalPatchRecoveryBase` | One point-neighbor layer of patch contributions | Algebraic RM plus `_Ae`/`_be` synchronization | No |
| EVBC | A manually gathered primary element and its DoFs are consumed later | Manual gather, explicit retention, and `addGhostedElem()` | Yes |

Thus, "one element far" is not the deciding condition. A normal stencil should
be expressed with the appropriate RelationshipManager and remote computed data
should be synchronized when possible. `add_extra_ghost_elem()` is for a remote
`Elem` that is outside geometric retention rules but whose object must remain
available after cleanup.

### 5. Why the fix belongs in EVBC rather than `FEProblemBase`

Putting `add_extra_ghost_elem()` inside `FEProblemBase::addGhostedElem()` is
attractive because it establishes the invariant centrally. It also changes the
behavior of every caller, including contact, nearest-node searches,
peridynamics, patch recovery, and other constraints. All of those callers would
begin retaining mesh elements, possibly for longer than intended, without tests
showing that they need that geometric behavior.

Putting the call in EVBC is more surgical:

- EVBC is the code that manually gathers the element.
- EVBC knows a later operation will consume its ID.
- Existing callers of `addGhostedElem()` keep their current behavior.
- The regression test proves this exact path rather than a broader API contract.
- It follows the ownership pattern demonstrated by `SampledOutput`.

The framework-level alternative would only be preferable after explicitly
changing and documenting the contract of `SubProblem::addGhostedElem()` to mean
both geometric retention and algebraic DoF ghosting, then testing its other call
sites.

### 6. Why missing elements must not be silently skipped

An attempted defensive change used `queryElemPtr()` in
`SystemBase::augmentSendList()` and continued when it returned null. That avoids
the crash but can silently omit required DoFs and produce an incorrect solve.
It hides the broken invariant rather than repairing it.

Keep the original fail-fast `elemPtr()` lookup. If a stored ID does not identify
an element, the code should fail rather than continue with incomplete ghosting.
The producer must ensure that the required element remains available.

### 7. Scope and verified behavior

The deterministic reproducer uses `TestGhostBoundarySideUserObject` to request
a late geometric relationship manager, selects primary node 120, and runs on
three ranks. The trigger schedules remote-element deletion without retaining
anything useful for this mesh.

Verified in the `moose` conda environment:

- Before retention was added, the focused three-rank case asserted in
  `distributed_mesh.C:492`.
- After retention was added, the direct three-rank case passed.
- `METHOD=dbg make -j 10` passed.
- The existing EVBC suite passed 6 tests.
- The local harness skipped the new manifest entry because it exposed fewer than
  three scheduler slots; the equivalent direct MPI command passed.
- Forcing the existing replicated adaptivity test to use a three-rank
  distributed mesh still asserted during mesh reinitialization. Distributed
  adaptivity is a separate unresolved path and is not claimed as fixed here.

Periodic BCs and `build_all_side_lowerd_mesh` were rejected as focused triggers:
periodic ghosting can retain the primary element itself, while the lower-D case
failed even with EVBC disabled.

### 8. Review checklist for similar bugs

When code manually gathers or inserts a remote element, ask:

1. What later code needs the `Elem` object?
2. What later code needs the element's DoFs?
3. Can `delete_remote_elements()` or repartitioning run between producer and
   consumer?
4. Is a `GhostingFunctor` or `RelationshipManager` already responsible for the
   element?
5. If not, should the owning component call `add_extra_ghost_elem()`?
6. Is the retained element exactly the one whose ID is stored?
7. Does a missing element fail loudly rather than silently skip required work?
8. Does the test include the operation that would otherwise delete the element?

## Historical investigation log

The sections below record how the mechanism was discovered. Some proposed fixes,
notably making the consumer null-safe and treating a periodic BC as the focused
trigger, are superseded by the tutorial above.

## 1. The mechanism

### 1.1 EVBC hand-ghosts an element and stores a bare ID

`EqualValueBoundaryConstraint::ghostPrimary()` runs from the constructor, i.e.
at the `add_constraint` task. **[verified]**

`framework/src/constraints/EqualValueBoundaryConstraint.C`:

```cpp
if (!_mesh.getMesh().is_serial())                       // :225
{
  // collect the elements attached to the primary node that this rank has
  for (dof_id_type id : node_to_elem_pair->second)
  {
    Elem * elem = _mesh.queryElemPtr(id);               // :239  (null-safe)
    if (elem) { primary_elems_to_ghost.insert(elem); ... }
  }

  // broadcast them to everybody
  _mesh.getMesh().comm().allgather_packed_range(&_mesh.getMesh(),
                                                nodes_to_ghost.begin(),  ...);   // :252
  _mesh.getMesh().comm().allgather_packed_range(&_mesh.getMesh(),
                                                primary_elems_to_ghost.begin(), ...);  // :257
  _mesh.update();                                       // :264
  ...
}
...
_subproblem.addGhostedElem(elems[0]);                   // :279
```

Two properties of this code are the root of the problem: **[verified]**

1. The elements are injected into the mesh **by hand**. No `GhostingFunctor` /
   `RelationshipManager` claims them, so nothing in libMesh knows they must be
   kept.
2. What survives the call is a **bare `dof_id_type`** in
   `SubProblem::ghostedElems()`, not a pointer and not a ghosting rule. There is
   no mechanism that invalidates that ID if the element later goes away.

`FEProblemBase::addGhostedElem` (`FEProblemBase.C:2234`) only stores the ID when
the element is off-rank -- i.e. it stores exactly the IDs that are most at risk:

```cpp
if (_mesh.elemPtr(elem_id)->processor_id() != processor_id())
  _ghosted_elems.insert(elem_id);
```

### 1.2 A later `deleteRemoteElements()` takes the element back

`SetupMeshCompleteAction.C:93-113` **[verified]**:

```cpp
else if (_current_task == "delete_remote_elements_after_late_geometric_ghosting")
{
  if (_mesh->needsRemoteElemDeletion())      // :104
  {
    _problem->updateMortarMesh();
    _mesh->deleteRemoteElements();           // :109
    ...
  }
}
```

Task order from `framework/src/base/Moose.C:440-490` **[verified]**:

```
... (add_constraint) ... (attach_geometric_rm_final)
    (delete_remote_elements_after_late_geometric_ghosting) (init_problem) ...
```

So the deletion happens strictly **after** the constraint has recorded its ID
and strictly **before** the systems are initialized. The hand-injected element
is not protected by any functor, so it is dropped.

`needsRemoteElemDeletion()` returns `MooseMesh::_need_delete`, which is set by
`MooseMesh::allowRemoteElementRemoval(false)` (`MooseMesh.C:4035-4046`).
This is the **complete** list of callers in `framework/` and `modules/`
**[verified by grep over the whole repo]**:

| trigger | call site |
| --- | --- |
| `Mesh/build_all_side_lowerd_mesh` | `MooseMesh.C:281` |
| `Mesh/displacements` (displaced problem) | `CreateDisplacedProblemAction.C:133` |
| `BCs/Periodic` | `AddPeriodicBCAction.C:54` |
| `RayBCs` periodic | `SetupPeriodicRayBCAction.C:44` |
| any geometric RM with `attach_geometric_early = false` | `MooseApp.C:3186`, `MooseApp.C:3226` |
| `Adaptivity` (sets `_need_delete` back to *false*) | `Adaptivity.C:320-321` |

The only classes that set `attach_geometric_early = false`, i.e. the only ones
that can hit the `MooseApp.C` rows, are `NodeFaceConstraint.C:51`,
`AugmentSparsityOnInterface.C:56` (mortar), `GhostLowerDElems.C:29`,
`GhostHigherDLowerDPointNeighbors.C:30`, `GhostAllPointNeighbors.C:31`,
`GhostEverything.C:29`, `CreateDisplacedProblemAction.C:64` (proxy RM), and
`DistributedRectilinearMeshGenerator.C:78`. Nothing in `modules/` sets it.
**[verified]** In particular `InterfaceKernelBase.C:69`, `DGKernelBase.C:56` and
`InterfaceMaterial.C:23` all add `ElementSideNeighborLayers` with the default
(early) setting, so interface/CZM objects do **not** arm the deletion.

This is why plain "EVBC + DISTRIBUTED" runs fine: without one of the rows above
`_need_delete` is false, nothing is deleted, and the stale ID never becomes
stale. **[verified by run: `distributed_evbc.i` passed on `-np 6` before a
`_need_delete` trigger was added.]**

### 1.3 The stale ID is dereferenced without a null check

`framework/src/systems/SystemBase.C:452-489` **[verified]**:

```cpp
std::set<dof_id_type> & ghosted_elems = _subproblem.ghostedElems();   // :455
for (const auto & elem_id : ghosted_elems)                            // :467
{
  Elem * elem = _mesh.elemPtr(elem_id);                               // :469
  if (elem->active())                                                 // :471
```

`MooseMesh::elemPtr()` forwards to `MeshBase::elem_ptr()`, which is **not**
null-safe on a `DistributedMesh` (`libmesh/src/mesh/distributed_mesh.C:491`):

```cpp
Elem * DistributedMesh::elem_ptr (const dof_id_type i)
{
  libmesh_assert(_elements[i]);        // :493  -- fires in dbg
  libmesh_assert_equal_to (_elements[i]->id(), i);
  return _elements[i];
}
```

`_elements` is a `mapvector`, so `_elements[i]` on a missing ID default-inserts
a null entry: **dbg asserts at `distributed_mesh.C:493`, opt returns `nullptr`
and segfaults at `SystemBase.C:471`.** `DisplacedSystem::augmentSendList`
(`DisplacedSystem.h:169`) forwards to the same function, so the displaced system
hits it too. **[verified]**

Call path to the crash, from the original stack in `README.md`:

```
FEProblemBase::init()
  EquationSystems::reinit_mesh()
    System::reinit_mesh()
      System::init_data()
        DofMap::prepare_send_list()
          SystemBase::augmentSendList()      <-- here
```

## 2. What the inputs in this directory establish

| input | result | what it tells us |
| --- | --- | --- |
| `distributed_evbc.i` (EVBC + BreakMeshByBlock, no `_need_delete` trigger) | passes on `-np 6` | EVBC alone on a distributed mesh is fine |
| `distributed_evbc.i` + `Mesh/displacements` | fails | needs a `_need_delete` trigger -- but see the caveat in section 3 |
| `distributed_evbc_no_break.i` + `Mesh/displacements` | fails | `BreakMeshByBlockGenerator` is **not** required |
| `minimal_evbc.i` | see section 3 | 2D `GeneratedMeshGenerator`, one variable, one EVBC |

`BreakMeshByBlockGenerator` was ruled out. It calls
`add_disjoint_neighbor_boundary_pairs()` (`BreakMeshByBlockGenerator.C:606`), so
`neighbor_ptr` survives across a broken interface and `SideSetsFromNormalsGenerator`
produces the same external-only sidesets with or without it. The only residual
difference is node renumbering, which changes *which* node is picked as primary
but not the failure mode. **[verified]**

## 3. Open item: the `distributed_mesh.h:123` assert is a different bug

When `Mesh/displacements` was used to arm the deletion, the reported dbg failure
was:

```
[0] ./include/libmesh/distributed_mesh.h, line 123
```

That line is **not** the `elem_ptr` assert. It is **[verified]**:

```cpp
virtual std::unique_ptr<MeshBase> clone () const override
{
  auto returnval = std::make_unique<DistributedMesh>(*this);
#ifdef DEBUG
  libmesh_assert(*returnval == *this);      // line 123
#endif
  return returnval;
}
```

`MeshBase::operator==` is collective (`locally_equals` then `comm().min(...)`,
`mesh_base.C:258`), which matches both ranks aborting together.

The only mesh `clone()` in a normal MOOSE run is
`MooseMesh::MooseMesh(const MooseMesh &)` (`MooseMesh.C:300`), reached through
`safeClone()` at `SetupMeshAction.C:366`, and that runs at the **`init_mesh`**
task -- *before* `add_constraint`. **[verified]** An `EqualValueBoundaryConstraint`
that has not been constructed yet cannot have caused it.

**Conclusion: using `Mesh/displacements` to arm the deletion introduced a second,
independent dbg-only failure (distributed mesh + displaced clone) that masks the
EVBC bug.** `minimal_evbc.i` has therefore been changed to use `BCs/Periodic`
instead, which sets `_need_delete` without cloning the mesh.

### What still needs to be run

See section 5.2. Nothing in this directory has been built or executed yet, so
every "expected" in sections 1-4 is **[unverified]**.

### 3.1 `original_creep.i` has no `_need_delete` trigger

Checking `original_creep.i` against the complete table in section 1.2
**[verified]**:

| trigger | present in `original_creep.i`? |
| --- | --- |
| `build_all_side_lowerd_mesh` | no |
| displaced problem | **no** -- `Mesh/use_displaced_mesh = false` (`:27`) gates `CreateDisplacedProblemAction.C:133` at `:104`, and `GlobalParams/displacements` cannot override that |
| `BCs/Periodic` | no |
| `RayBCs` periodic | no |
| late geometric RM | no -- the only RMs the input can produce come from `CohesiveZoneAction.C:194` and `InterfaceMaterial.C:23`, both `ElementSideNeighborLayers` with `attach_geometric_early` left at its `true` default |

Nothing else deletes elements in the window either. Between `add_constraint` and
`SystemBase::augmentSendList()` the only mesh work is, in order **[verified]**:

- `FEProblemBase::init()` -> `ghostGhostedBoundaries()` (`FEProblemBase.C:6801`) --
  *adds* elements, never removes
- `FEProblemBase::init()` -> `_mesh.meshChanged()` (`FEProblemBase.C:6808`) --
  `MooseMesh::meshChanged()` (`MooseMesh.C:887`) only calls `update()` and
  rebuilds cached ranges; it does not call `prepare_for_use()`
- `es().init()` (`FEProblemBase.C:6841`) -> `EquationSystems::init()`
  (`equation_systems.C:87`) -> `reinit_mesh()` (`:102`) -- sets `n_systems` on
  each DofObject and cleans refinement flags, nothing more

**So the section 1 mechanism does not, on its own, explain the original crash.**
Either the user's application adds a late geometric RM through an object that is
not in this repository (`NEMLCrystalPlasticity`, `GrainBoundaryCavitation`,
`PropertyReadFile`), or the original failure has a different immediate cause.

How to settle it quickly: the MOOSE console header prints a
`Relationship Managers:` block (`Console.C:749`, fed by
`MooseApp::getRelationshipManagerInfo()`, `MooseApp.C:3319`) listing every RM and
which object asked for it. Anything in that list other than
`ElementSideNeighborLayers` / the default functors -- in particular
`GhostEverything`, `GhostAllPointNeighbors`, `GhostLowerDElems`,
`GhostHigherDLowerDPointNeighbors`, `AugmentSparsityOnInterface`, or
`ProxyRelationshipManager` -- is the missing trigger.

Note also that `original_creep.i` never sets `Mesh/parallel_type`, so the
original run must have used `--distributed-mesh` on the command line.

## 4. Hints for a fix

Independent layers. A0 is applied; A is the more thorough alternative to A0; B
is a cheap safety net that also protects the other callers in C.

### A0. Register the gathered element in `_extra_ghost_elems` -- recommended EVBC fix

This is the smallest targeted fix and is the implemented placement. MOOSE
already uses the same retention API when a component knows that it must keep
specific elements.

`MooseMesh::ghostGhostedBoundaries()` (`MooseMesh.C:3479-3488`) **[verified]**:

```cpp
mesh.comm().allgather_packed_range(&mesh,
                                   connected_nodes_to_ghost.begin(),
                                   connected_nodes_to_ghost.end(),
                                   extra_ghost_elem_inserter<Node>(mesh));
mesh.comm().allgather_packed_range(&mesh,
                                   boundary_elems_to_ghost.begin(),
                                   boundary_elems_to_ghost.end(),
                                   extra_ghost_elem_inserter<Elem>(mesh));
```

`EqualValueBoundaryConstraint::ghostPrimary()` (`:255`, `:260`) passes
`libMesh::null_output_iterator<Node>()` / `<Elem>()` instead. The difference
matters: `extra_ghost_elem_inserter` registers the element in
`DistributedMesh::_extra_ghost_elems`, and that set is exactly what protects
elements from deletion (`libmesh/src/mesh/distributed_mesh.C:1732`):

```cpp
MeshCommunication().delete_remote_elements(*this, _extra_ghost_elems);
```

So EVBC pulls the element back but never tells the mesh to keep it, while
`ghostGhostedBoundaries()` does.

A literal swap of the two output iterators is not possible:
`extra_ghost_elem_inserter` is declared in an anonymous namespace in
`MooseMesh.C:3352-3411`, so it is not visible outside that translation unit.
Exposing it would mean moving it to a header, which is a bigger change than the
bug warrants. Two further observations make the swap unnecessary anyway
**[verified]**:

- `Packing<Node *>::unpack()` already calls `mesh->add_node(...)`
  (`libmesh/src/parallel/parallel_node.C:267`), so the objects land in the mesh
  regardless of which output iterator is used. `null_output_iterator` is not
  discarding them; the only thing it discards is the `add_extra_ghost_elem()`
  side effect for elements.
- `extra_ghost_elem_inserter<Node>` only re-calls `add_node()`
  (`MooseMesh.C:3370`), which is redundant here. Only the `Elem` overload
  (`MooseMesh.C:3368`) does anything the constraint needs.

So the applied fix registers just the one element that is actually recorded:

```cpp
if (auto * const distributed_mesh = dynamic_cast<DistributedMesh *>(&_mesh.getMesh()))
  distributed_mesh->add_extra_ghost_elem(_mesh.elemPtr(elems[0]));
```

`dynamic_cast` rather than an unconditional cast because `add_extra_ghost_elem`
is declared only on `DistributedMesh` (`libmesh/include/mesh/distributed_mesh.h:252`),
not on `MeshBase`.

One thing to watch if this ever runs under adaptivity:
`ghostGhostedBoundaries()` records what it added
(`_ghost_elems_from_ghost_boundaries`, `MooseMesh.C:3490-3497`) so it can
`clear_extra_ghost_elems()` them on the next call (`MooseMesh.C:3432`). A
constraint constructed once does not need that bookkeeping, but a re-entrant
caller would leak entries into `_extra_ghost_elems`.

### A. Alternative: do not hand-ghost at all -- use a RelationshipManager

The correct way for a constraint to keep an element alive across
`deleteRemoteElements()` is to add a geometric `RelationshipManager` /
`GhostingFunctor` for it, so libMesh preserves it instead of MOOSE re-adding it
behind libMesh's back. `NodeFaceConstraint` already does this
(`NodeFaceConstraint.C:51` adds an RM with `attach_geometric_early = false`),
and that is the pattern to follow.

Because the primary node is only known at construction time, the functor needs
to be a small one that ghosts a single, dynamically-set element ID -- something
like a `GhostElemById`-style functor whose `operator()` yields the recorded
element. That keeps the element alive through the deletion *and* removes the
need for the `allgather_packed_range` + `_mesh.update()` dance entirely.

Note the current code also has a correctness gap independent of the crash:
`allgather_packed_range()` injects nodes and elements into a prepared mesh and
then only calls `MooseMesh::update()`, which rebuilds the node-to-elem map but
does **not** call `prepare_for_use()` or `update_parallel_id_counts()`.
`MeshRepairGenerator.C:196` is the one place in MOOSE that does call
`update_parallel_id_counts()` after mutating a distributed mesh. **[verified]**
If layer A is not adopted, that call is probably needed here too.

### B. Make the consumer null-safe

`SystemBase.C:469` should not dereference an ID it does not own:

```cpp
Elem * elem = _mesh.queryElemPtr(elem_id);   // instead of elemPtr()
if (!elem)
  continue;                                  // or mooseError with a real message
if (elem->active())
  ...
```

`queryElemPtr()` (`MooseMesh.C:3225`) forwards to `MeshBase::query_elem_ptr()`,
which returns `nullptr` instead of asserting. This turns a segfault into either
a silent skip or an actionable error. Note that `ghostPrimary()` itself already
uses `queryElemPtr()` at line 239 -- the null-safety is applied on the producing
side but not on the consuming side.

The same treatment protects `FEProblemBase::addGhostedElem`
(`FEProblemBase.C:2237`), which also uses the unsafe `elemPtr()`.

### C. Audit the other bare-ID storers

The same "store an element ID and hope it is still there" pattern appears at
**[verified]**:

- `LinearNodalConstraint.C:90`
- `EqualValueEmbeddedConstraint.C:93`
- `NearestNodeLocator.C:156`, `:161`, `:371`

`LinearNodalConstraint` in particular looks like it has the identical bug and
should get a test alongside whatever is added for EVBC.

### D. Test to add once fixed

`minimal_evbc.i` is small enough to become a regression test. It needs a
`min_parallel = 2` and `mesh_mode = DISTRIBUTED` entry in `tests`. The existing
`adaptivity` test in
`test/tests/constraints/equal_value_boundary_constraint/tests` is pinned to
`mesh_mode = REPLICATED`, which is a hint that distributed EVBC has never been
covered.

## 5. Current stage and next steps

### 5.1 Current worktree state

| file | change | state |
| --- | --- | --- |
| `framework/src/constraints/EqualValueBoundaryConstraint.C` | adds geometric retention for the manually gathered primary element before algebraically ghosting its DoFs | recommended targeted design |
| `minimal_evbc.i` | deterministic late-geometric-RM reproducer with primary node 120 | direct three-rank run passes with retention and failed before it |
| `tests` | adds an exactly-three-rank distributed regression | local harness skips for insufficient slots; equivalent direct run passes |
| `distributed_evbc.i` | investigation input with unrelated local changes | not part of the focused regression |
| `distributed_evbc_no_break.i` | investigation input without `BreakMeshByBlockGenerator` | not part of the focused regression |
| `NOTES.md` | tutorial plus historical investigation | current |

The retention call is implemented in
`EqualValueBoundaryConstraint::ghostPrimary()` as explained in tutorial section
4; `FEProblemBase::addGhostedElem()` retains its original algebraic-only behavior.

### 5.2 Remaining next steps

1. Run the manifest test on a machine where the harness exposes at least three
   MPI slots.
2. Treat distributed adaptivity as a separate investigation; forcing the
   replicated adaptivity input to distributed mode currently still asserts.
3. Audit `LinearNodalConstraint` separately rather than broadening this fix
   without a reproducer for that class.

### 5.3 `original_creep.i` -- blocked, and probably a separate investigation

It cannot be run here: `NEMLCrystalPlasticity` and `GrainBoundaryCavitation` do
not exist anywhere in this repository (grep confirms they appear only in
`original_creep.i` and in these notes), so `moose_test-dbg` will fail at object
registration, not at the constraint. It needs the user's own application binary.

More importantly, section 3.1 shows the input arms no late `deleteRemoteElements()`,
so **the patch in 4.A0 is not expected to change its behaviour**. Before spending
build cycles on it, get the `Relationship Managers:` block from the console
header of the original failing run. That single block decides whether:

- a late geometric RM from the user's app is the missing `_need_delete` trigger
  (then 4.A0 should fix it, and the mechanism in section 1 is complete), or
- there is no such RM (then the original crash has a different immediate cause
  and section 1 explains only the synthetic reproducer).

Also worth capturing from that run: the exact command line (the input never sets
`Mesh/parallel_type`, so `--distributed-mesh` must have been passed), and a real
backtrace.
