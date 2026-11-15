# NEML2 batch maps for Kokkos gathering

## Purpose

Prepare `NEML2BatchIndexGenerator` batch-index data for the later
`KokkosQuantityToNEML2` implementation without yet adding a Kokkos/Torch gather buffer, a
material-property gatherer, or SCZM kinematics.

The next gatherer must write a Kokkos-produced value into the same NEML2 batch row selected by the
existing host path. It must not infer that row from element IDs, contiguous Kokkos IDs, interface
face-cache order, or a device-side append counter. This slice establishes that contract and proves
that the authoritative host map can be mirrored safely for device consumers.

This is a prerequisite of the unified plan in `kokkos_quantity_to_neml2.md`, specifically its former
Phase 3. That broader phase is deliberately split here: batch-map ordering/lifetime is reviewed and
verified before gatherer storage, `at::from_blob`, and NEML2 history ownership are introduced.

## Scope

### In scope

1. Make Action-created `NEML2BatchIndexGenerator` objects execute before gatherers and executors.
2. Add generation-tracked, device-readable mirrors of the generator's authoritative maps.
3. Define lookup behavior required by future bulk and interface Kokkos gatherers.
4. Rebuild mirrors when the generator rebuilds its host maps, including mesh changes.
5. Add focused CPU Kokkos tests for map contents, generation refresh, and execution ordering.
6. Preserve current host gatherer, executor, output-retriever, restart, and state-remapping behavior.

### Explicitly out of scope

- `KokkosQuantityToNEML2` classes or registration.
- Kokkos material-property reads or rank-two packing.
- Kokkos/Torch buffer wrapping, `at::from_blob`, fences, or CUDA stream interoperability.
- NEML2 accepted-history snapshots and cutback tests.
- `input_backend` selection in `NEML2Action`.
- Kokkos deformation-gradient production and SCZM fixture migration.
- Small-strain Mandel packing and variable-input migration.
- Any change to public NEML2 batch ordering.

## Existing contracts

### Authoritative host generator

`NEML2BatchIndexGenerator` is a `DomainUserObject` that currently owns:

```cpp
std::map<dof_id_type, std::size_t> _elem_to_batch_index;
StateBatchIndexMap _elemside_to_batch_index;
std::size_t _batch_index;
bool _outdated;
```

The generator exposes:

- `getBatchIndex(elem_id)` for volume batches;
- `getSideBatchIndex({elem_id, side})` for interface-side batches;
- `isSideBatchIndexExist({elem_id, side})`;
- `getBatchIndex()` for total batch size;
- `meshChanged()` to mark the mapping outdated.

The host `NEML2ModelExecutor` already relies on this map for output retrieval and state remapping.
Those host maps remain the source of truth. The device representation is a mirror, not a second
ordering algorithm.

### Bulk mapping

For a bulk element/QP, the future device gatherer must obtain the batch segment selected by the
host volume map:

```text
(element ID, QP) -> host generator volume batch index -> device-visible destination offset
```

The exact offset arithmetic must match the existing host gatherer/NEML2 tensor layout. This plan must
locate that arithmetic before choosing a mirror layout; it must not add a parallel convention.

### Interface mapping

For an oriented cached interface face and primary quadrature point:

```text
(face.elem, face.side, primary_qp)
  -> host generator primary side batch index
  -> primary destination offset

(face.neighbor, face.neighbor_side, mapped_neighbor_qp)
  -> host generator neighbor side batch index
  -> neighbor destination offset
```

The interface face cache's `neighbor_qp_map` chooses where the neighbor property is read. It does
**not** choose the NEML2 destination row. The generator's primary/neighbor side entries choose those
rows. Face-cache traversal order is not batch order.

Before implementation, inspect the exact keys passed to `getSideBatchIndex()` by the host
`MOOSEQuantityToNEML2::executeOnInterface()` path. Do not assume an `InterfaceFaceRecord` member name
or orientation is identical to the host traversal without checking it.

### Execution ordering

The required order is:

```text
execution_order_group = -1: NEML2BatchIndexGenerator
execution_order_group =  0: Kokkos gatherers, then NEML2ModelExecutor
```

The generator must complete before a Kokkos gatherer copies or reads device offsets. The underlying
`FEProblemBase::computeUserObjects()` order is material:

```text
for each execution group:
  computeKokkosUserObjectsInternal(group)
  computeUserObjectsInternal(group)
```

`NEML2BatchIndexGenerator` is a host `DomainUserObject`. Therefore, if it and a future Kokkos gatherer
share group 0, the gatherer necessarily runs first and observes an uninitialized or stale mirror. Group
`-1` is required, not a preference. Within group 0, Kokkos gatherers then run before the normal host
`NEML2ModelExecutor` path. The executor's existing explicit gatherer dependencies remain an additional
ordering guard; they are not a substitute for the earlier generator group.

The Action must assign this ordering for Action-created NEML2 objects. Also evaluate setting the
`NEML2BatchIndexGenerator::validParams()` default `execution_order_group` to `-1` so manually declared
`[UserObjects]` generators cannot accidentally use the unsafe default group. Retain an explicit Action
assignment as documentation and protection if a caller overrides defaults.

## Design decisions for this slice

1. **The host map remains canonical.** Device arrays are built only from finalized generator maps.
2. **No device `std::map` or host map access.** A Kokkos kernel sees only Kokkos-accessible compact
   arrays and scalar metadata.
3. **No reliance on raw libMesh element IDs as dense array indices.** IDs may be sparse, nonlocal, or
   changed by repartitioning. The bulk mirror may use an existing Kokkos mesh contiguous element ID or
   another established device-visible translation, then explicitly build the corresponding lookup
   array from the canonical host map.
4. **Interface lookup is face-cache aligned, not element-side dense.** The interface mirror contains
   one primary and one neighbor batch index for each cached oriented interface face. Its two arrays are
   indexed directly by `InterfaceFaceRecord::face_index`/the interface-loop `tid`; an interface kernel
   must not allocate `[all local-and-ghost elements, max sides]` storage merely to represent a sparse
   cohesive boundary.
5. **Do not duplicate batch-order logic on device.** Device data stores resolved offsets/index values,
   not an algorithm that recreates host map insertion order.
6. **A generation identifies a complete mirror.** Increment it only after host maps and all device
   mirror arrays are mutually consistent. Gatherers may compare a recorded generation to detect stale
   views after later allocation/rebuild work is added.
7. **An empty batch has a valid empty mirror.** Empty arrays, zero total batch size, and a current
   generation must be handled without stale allocations or invalid device reads.
8. **Build after finalization, not incrementally in threaded callbacks.** `executeOnElement()`,
   `executeOnBoundary()`, and `executeOnInterface()` may execute in threaded traversal. Build compact
   device mirrors only after `finalize()` has established the complete canonical maps.
9. **Do not copy a current mirror.** `_outdated` gates the entire rebuild: mirror allocation, host fill,
   device deep copy, and generation increment are all no-ops when the current host map remains valid.
10. **Existing host accessors retain their current semantics.** This slice adds device-facing accessors;
   it does not replace host maps, caches, state keys, or restart data.
11. **Kokkos is optional.** All Kokkos includes, fields, accessors, and device lookup helpers are fully
   guarded by `#ifdef MOOSE_KOKKOS_ENABLED`; a NEML2-enabled non-Kokkos build retains the current host
   generator API and behavior.

## Proposed representation

Use separate representations for volume and interface domains.

The volume mirror may be dense over **contiguous Kokkos element identity**, never raw element ID:

```text
bulk_batch_index[contiguous_elem] -> volume batch index or invalid sentinel
```

The interface mirror must be sparse and aligned exactly to `InterfaceFaceCache`:

```text
face_primary_batch_index[face_index] -> primary side batch index or invalid sentinel
face_neighbor_batch_index[face_index] -> neighbor side batch index or invalid sentinel
```

Each interface array has exactly `numKokkosInterfaceFaces()` entries. `face_index` is already the
parallel-loop index used by `KokkosInterfaceUserObject` and `KokkosInterfaceMaterial`, so this avoids a
mostly-sentinel `[num local-and-ghost elements, max sides]` GPU table and gives each interface thread a
direct O(1) lookup. Build the values by resolving the host generator's element-side keys for the primary
and neighbor sides of each cached face; the face index is only the device mirror lookup key, never a new
batch-order definition.

Store the following immutable metadata with the arrays:

```text
batch_size
map_generation
invalid_batch_index
number_of_cached_interface_faces
```

The generator/mirror builder must request
`_fe_problem.mesh().getKokkosMesh()->setNeedsElementSideGeometry()` whenever it constructs an interface
face mirror. This is required before Kokkos mesh initialization so the device-visible mapping includes
one-layer ghost neighbor elements on distributed meshes. Reuse the geometry request already required by
`KokkosInterfaceUserObject`; do not assume an existing interface user object happens to be present.

If the current Kokkos mesh does not provide the necessary device-visible contiguous element ID for all
locally relevant and one-layer ghost elements, stop and choose an existing supported translation. Do not
introduce a new raw-ID hash table or a second ghosting mechanism casually.

The API should make the distinction between a batch **index** and a final tensor **offset** explicit.
For example, this slice may expose device-readable batch indices and total batch size, while the later
gatherer owns rank-dependent QP/component offset arithmetic. Alternatively, if the existing host
contract already exposes final offsets, mirror those exact offsets. Decide from source inspection and
record the result in code comments and tests.

Potential API shape (names are illustrative, not prescribed):

```cpp
std::size_t mapGeneration() const;
#ifdef MOOSE_KOKKOS_ENABLED
const Moose::Kokkos::Array<std::size_t> & kokkosElementBatchIndices() const;
const Moose::Kokkos::Array<std::size_t> & kokkosFacePrimaryBatchIndices() const;
const Moose::Kokkos::Array<std::size_t> & kokkosFaceNeighborBatchIndices() const;
#endif
std::size_t invalidBatchIndex() const;
```

`KokkosArray.h`, all Kokkos fields, and all KOKKOS_FUNCTION helpers follow the same
`MOOSE_KOKKOS_ENABLED` guard. The host-only `mapGeneration()` and invalid-sentinel API may remain
available if useful, but a non-Kokkos NEML2 build must neither include Kokkos declarations nor require
Kokkos types to parse this header.

Do not expose mutable mirror storage to consumers. A later Kokkos gatherer receives shallow device
handles in its functor copy, records `mapGeneration()`, and rebuilds its own dispatcher/buffer views
when that generation changes.

## Implementation sequence

### 1. Trace and document current map semantics

Before edits, inspect:

- `NEML2BatchIndexGenerator::initialize`, element/boundary/interface callbacks, `finalize`, and
  `meshChanged`;
- `MOOSEQuantityToNEML2` bulk and `executeOnInterface()` paths;
- `NEML2ModelExecutor` map use, `meshChanged`, and state remapping;
- `NEML2Action` creation of the generator, gatherers, and executor;
- Kokkos mesh facilities for local plus one-neighbor-layer ghost contiguous element IDs;
- any existing generation-tracked Kokkos array/cache pattern in framework code.

Write a short implementation note in the new code or test documenting:

| Domain | Canonical host key | Device key | Value | Invalid behavior |
|---|---|---|---|---|
| bulk | element ID | established contiguous Kokkos element ID | batch index/offset | sentinel |
| interface primary | element ID + side | cached `face_index` | primary side batch index/offset | sentinel |
| interface neighbor | neighbor element ID + side | cached `face_index` | neighbor side batch index/offset | sentinel |

If interface-side maps contain only the primary side, only neighbor sides, or a combination that
depends on `interface_only`, record and test it rather than generalizing from one fixture.

### 2. Set Action execution groups

Modify `NEML2Action` only where it creates the relevant objects:

1. Set the generated `NEML2BatchIndexGenerator` parameter:
   ```text
   execution_order_group = -1
   ```
2. Evaluate and, unless an existing compatibility requirement forbids it, set the generator's
   `validParams()` default to `-1` too. This makes manually declared `[UserObjects]` safe rather than
   leaving their correctness dependent on a user knowing the Kokkos-before-host scheduler detail.
3. Keep the executor in group `0`.
4. Do not add Kokkos gatherer creation yet. Reserve group `0` for it and document the required order.
5. Ensure host-only Action paths retain equivalent generator-before-executor ordering.

Do not add a user-facing ordering parameter. The ordering is an internal correctness requirement.

### 3. Add generator generation and mirror lifecycle

Extend `NEML2BatchIndexGenerator`:

1. Add a monotonically increasing generation field initialized to a defined value.
2. Add Kokkos-capable storage only under `#ifdef MOOSE_KOKKOS_ENABLED` and the framework's established
   guarded include pattern. A non-Kokkos build must preserve current behavior and public availability.
3. When interface boundaries are configured, request
   `_fe_problem.mesh().getKokkosMesh()->setNeedsElementSideGeometry()` during setup, before Kokkos mesh
   initialization. This is required even if no other Kokkos interface object is present: distributed
   interface mirrors must translate ghost-neighbor geometry/identity without missing-key failures.
4. In `initialize()`, preserve the existing early `if (!_outdated) return;` behavior, then clear
   canonical maps and invalidate/prepare mirror state. It must not invalidate a current published mirror
   on ordinary linear/nonlinear executions.
5. In `finalize()`, use the same outdated guard: if the host map was current on entry, immediately
   return without allocation, host fill, device deep copy, fence, or generation increment. When a map
   rebuild occurred, after canonical map completion:
   - allocate/reallocate the compact bulk array and the two `face_index`-sized interface arrays;
   - initialize entries to the invalid sentinel;
   - translate each canonical bulk key to its device identity;
   - for every cached interface face, resolve the canonical primary and neighbor element-side entries
     and fill the same-index primary/neighbor arrays;
   - copy all host values to device;
   - increment/publish the generation only after copies finish.
6. In `meshChanged()`, invalidate the mirror as well as the host map. Do not leave an old device array
   advertised as current.
7. Verify that unchanged `EXEC_INITIAL`, `EXEC_LINEAR`, and `EXEC_NONLINEAR` invocations do not copy or
   increment the generation; generation changes exactly once per successful actual rebuild.

The implementation may use Kokkos arrays with host and device allocations, but must preserve lifetimes
across device functor copies. Do not return temporary views.

### 4. Add device-safe lookup helpers

Provide a minimal lookup interface suitable for the later gatherer:

- a KOKKOS_FUNCTION bulk lookup from a device `Datum`/Kokkos element identity;
- KOKKOS_FUNCTION primary and neighbor interface lookups directly from `face_index`;
- explicit invalid-sentinel checks or a separate existence helper.

The interface helper intentionally takes no element/side lookup key: the cached face has already
identified both oriented sides, and the two face-index arrays contain their resolved canonical values.

The helper must not throw, allocate, access `std::map`, call host-only `FEProblemBase`, or depend on a
host cache. Setup-time consumers can validate coverage on host; device code should use an assertion or
abort only for an invariant violation that should have been excluded during setup.

Do not build a gatherer in this commit. Add a test-only `KokkosBatchIndexMapVerifier` (or equally clear
name) as the explicit device consumer. Follow the existing `KokkosInterfaceUserObjectCounter` pattern:
it receives the generator's immutable mirror handles and expected setup data, looks up the applicable
index from each bulk `Datum` or interface `face_index` in device code, aggregates deterministic checks,
and exposes a Postprocessor/CSV result for `CSVDiff`. It must only observe/validate indices; it must not
allocate a NEML2 input tensor, read a material property, or introduce any gatherer behavior.

### 5. Test the contract

Add focused regression coverage in the existing NEML2/Kokkos test area or the closest existing
batch-index test location. Test names should describe the ordering behavior rather than the later
feature name.

Required cases:

1. **Bulk map values:** `KokkosBatchIndexMapVerifier` observes values in device code on a mesh whose
   libMesh element IDs cannot be mistaken for traversal or contiguous Kokkos order. Verify every result
   matches the canonical host generator map.
2. **Interface primary/neighbor values:** use a multi-face interface with at least two QPs per face.
   The verifier must use the primary and neighbor `face_index` arrays directly and verify both resolved
   values against the generator-defined element-side map. Neighbor source locations must still follow
   the existing mapped neighbor-QP path; destinations remain generator-selected.
3. **Traversal-order independence:** choose or perturb a fixture such that cached face order is not
   enough to predict expected rows. Verify explicit expected host-map values, not merely contiguous
   counts; this proves face index is only a mirror lookup and not an ordering algorithm.
4. **Generation refresh:** force the documented map regeneration path (prefer a supported mesh change,
   element activation, or an existing test fixture that calls `meshChanged`). Verify generation changes
   exactly when the map is rebuilt, device-observed entries update, and repeated unchanged execute calls
   neither deep-copy nor increment generation. If no focused runtime mesh-change fixture exists, add the
   smallest C++ unit test for generator/mirror rebuilding; do not fake a rebuild by mutating a private
   member.
5. **Execution ordering:** an Action-created configuration must prove generator execution completes
   before the first verifier read. The verifier fails deterministically on an invalid/unpublished
   generation; this specifically covers host generator group `-1` before Kokkos verifier group `0`, not
   timing-sensitive output.
6. **Empty batch:** if existing NEML2 setup supports it, verify zero-size map/mirror behavior. Otherwise
   document why the normal construction prevents it and do not invent unsupported input syntax solely
   for this case.
7. **Distributed interface:** run the interface verifier with two MPI ranks and distributed mesh. It
   must cover a neighbor element ghosted on the primary-face owner, proving the required
   `setNeedsElementSideGeometry()` setup supplies the neighbor geometry/identity mapping.

CPU Kokkos/OpenMP tests prove mapping and lifecycle only. They do not prove CUDA accessibility,
allocation residency, synchronization with Torch, or the absence of host staging.

## Verification sequence

Use the established `moose` conda environment and local harness. Do not build/test until this coherent
slice is complete.

1. `clang-format` changed C++ headers and sources.
2. `git diff --check`.
3. `./scripts/mac/moose.sh compile framework`.
4. Run only the new focused batch-map tests first, including the two-rank distributed variant with
   adequate TestHarness slots.
5. Run the relevant existing NEML2 host batch-index/state-remapping tests to detect an ordering or map
   regression.
6. Review the final diff for accidental changes to host generator ordering, map keys, executor state
   remapping, output retrievers, or Action input semantics.

Report precisely:

- CPU/OpenMP compilation and test results;
- serial and distributed test results;
- whether map-generation refresh was exercised;
- that CUDA qualification remains pending.

## Stop and ask before widening

Stop for design review rather than guessing if implementation requires any of the following:

- changing the public or persisted NEML2 batch ordering;
- using raw element IDs as dense Kokkos-array indices;
- creating a new device map/lookup mechanism when an established Kokkos mesh translation exists;
- changing `NEML2ModelExecutor` state remapping or restart keys merely to accommodate the mirror;
- making a Kokkos-specific array part of a required non-Kokkos build path without an existing guarded
  pattern;
- adding a new user-visible execution-order option;
- scheduling the generator and gatherer in the same group and relying on incidental traversal order;
- adding Torch storage, `from_blob`, host staging, or material-property gathering to this commit.

## Definition of done

This slice is complete when:

- Action-created generator runs in group `-1`, before consumer/executor group `0` work.
- The canonical host bulk map and interface side-map values are mirrored into stable device-readable
  storage: bulk by established Kokkos element identity, interfaces by two `face_index`-aligned arrays.
- Interface storage is proportional to cached interface faces, not all local-and-ghost elements times
  their possible sides; each interface device thread reads its primary/neighbor value by `face_index`.
- Interface setup requires element-side geometry before Kokkos mesh initialization, including ghost
  neighbor mapping on distributed meshes.
- Kokkos declarations and storage are completely guarded for non-Kokkos NEML2 builds.
- The mirror publishes a monotonic generation only after a complete rebuild/copy and invalidates on mesh
  change; unchanged executions perform no copy or generation increment.
- Focused tests prove bulk and interface values, mapped-neighbor-side use, traversal-order independence,
  first-execution ordering, map regeneration, and distributed neighbor coverage.
- Existing host NEML2 map consumers and state-remapping behavior remain unchanged.
- No gatherer, Torch buffer, history behavior, SCZM material, or CUDA claim is added.
