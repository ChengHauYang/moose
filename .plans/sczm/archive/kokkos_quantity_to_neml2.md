# Unified Kokkos-to-NEML2 input gathering for bulk and interfaces

## Outcome

Replace the split Kokkos input design with one `KokkosQuantityToNEML2` family that gathers
Kokkos-resident material properties directly into NEML2 input tensors for both bulk elements and
interfaces. The interface path gathers primary and neighbor values into the existing NEML2 batch
ordering. On CUDA, the tensor wraps the Kokkos device buffer with `at::from_blob`; it must not stage
through host memory.

This plan covers input gathering plus the device deformation-gradient producer required to exercise
that path end to end. Existing `NEML2ToKokkosMaterialProperty` output retrievers and the automatic SCZM
PK2-to-PK1 conversion remain in place.

## Fixed design decisions

1. `KokkosQuantityToNEML2` supports both bulk and interface material inputs.
2. Do not use `TorchFEInterpolation`. It gathers FE solution/gradient data for Torch FEM and does not
   provide the material-property, state, or NEML2 batch semantics required here.
3. Do not retain `KokkosSmallStrainToNEML2` as a separate permanent bulk path, but do not pretend
   that a nine-component deformation-gradient gatherer preserves its six-component Mandel-strain
   contract. Retiring it requires a Kokkos strain-producing material plus symmetric/Mandel packing in
   the unified family. Until that parity exists, keep the old gatherer and treat retirement as a
   separate migration gate rather than part of the first deformation-gradient implementation.
4. Keep one public gatherer contract, but do not force element and interface traversal through one
   oversized dispatcher. Share transfer/storage logic while using the appropriate Kokkos dispatch
   implementation for each domain.
5. One interface gatherer writes both primary and neighbor entries. A `neighbor` template parameter is
   inappropriate for input gathering because it would split a tensor that must be assembled as one
   NEML2 input.
6. `NEML2BatchIndexGenerator` is the source of truth for destination offsets. Traversal order must not
   define tensor order.
7. The first implementation supports current-state, full rank-two Kokkos material properties. For an
   interface, it supports block-material `FACE_MATERIAL_DATA` on the primary side and
   `NEIGHBOR_MATERIAL_DATA` on the neighbor side. True `INTERFACE_MATERIAL_DATA` has one interface
   value in the existing host contract and is out of scope until its desired two-segment mapping is
   defined. `interface_only` selects the batch domain; it does not identify property storage.
8. Input backend selection is explicit because `NEML2Action` creates gatherers before materials exist
   and cannot infer property residency. Add an input-side backend parameter independent of
   `output_backend`; never infer one from the other.
9. NEML2 still executes through Torch. "Direct" or "zero-copy" refers specifically to wrapping the
   Kokkos device allocation rather than copying Kokkos device data through host memory.
10. `Kokkos::fence()` is required before exposing the buffer to Torch because Kokkos and Torch may use
    different CUDA streams.
11. A tensor retained as NEML2 history must own an immutable snapshot. A `from_blob` view of a gather
    buffer that will be overwritten cannot be stored directly as accepted history.
12. CPU Kokkos tests prove mapping and lifecycle behavior, not CUDA interoperability. Do not claim CUDA
    qualification until CUDA compilation and execution pass in an NVIDIA environment.

## Target architecture

```text
                                  shared tensor/buffer contract
                            +-------------------------------------+
Kokkos bulk property ------>| KokkosQuantityToNEML2 implementation|----> at::Tensor ----> NEML2
                            | - NEML2 name and shape               |
Kokkos interface primary -->| - NEML2BatchIndexGenerator offsets  |
Kokkos interface neighbor -->| - Kokkos device buffer             |
                            | - fence + at::from_blob             |
                            | - gatheredData()                     |
                            +-------------------------------------+
                               ^                           ^
                               |                           |
                     element dispatch             interface-face dispatch
```

The implementation should use the smallest hierarchy supported cleanly by MOOSE registration and
execution scheduling. The preferred shape is a shared implementation/base plus separate concrete
dispatch objects when one concrete object cannot safely participate in both execution modes:

```text
KokkosQuantityToNEML2Base                 (shared storage and tensor exposure)
├── KokkosElementQuantityToNEML2          (bulk element/qp dispatch)
└── KokkosInterfaceQuantityToNEML2        (primary + mapped-neighbor face dispatch)
```

The exact internal class names may follow established MOOSE registration conventions, but the feature
and user-facing concept remain `KokkosQuantityToNEML2`. Do not introduce separate public semantics for
the two paths unless framework constraints require distinct registered object types.

## Required data mappings

### Bulk

For every active element and quadrature point:

```text
source:      bulk material property(elem, qp)
destination: NEML2BatchIndexGenerator bulk offset(elem, qp)
```

The destination index must match the ordering already consumed by bulk NEML2 output retrievers and
state remapping.

### Interface

For every cached interface face and primary quadrature point:

```text
primary source:       interface primary property(face, primary_qp)
primary destination:  side primary batch offset(elem, side, primary_qp)

neighbor source:      interface neighbor property(face, mapped_neighbor_qp)
neighbor destination: side neighbor batch offset(elem, side, primary_qp)
```

The primary quadrature point identifies the corresponding batch pair. The face cache's neighbor-QP map
selects the neighbor material storage location. Face traversal order must not be used as the batch
index.

## Implementation plan

### Phase 1: lock down contracts and migration scope

Use branch `neml2-v3-kokkos-solid-mechanics-gpu-transfer` at commit
`258b2838722286e21d32e2e59929591f1391a818` as the pinned comparison point. It contains both
`KokkosSmallStrainToNEML2` and an element-only, variable-input `KokkosQuantityToNEML2`; neither may be
silently replaced without accounting for its contract.

1. Inspect both pinned implementations and all callers.
2. Record the reusable behavior that must survive migration:
   - device buffer ownership and tensor options, device, scalar type, and shape;
   - vector-tag or execution dependency registration;
   - synchronization before `at::from_blob`;
   - behavior on non-CUDA Kokkos builds;
   - NEML2 gatherer interface and executor scheduling;
   - current/old/older variable support in the pinned `KokkosQuantityToNEML2`.
3. Record the incompatible contracts explicitly:
   - `KokkosSmallStrainToNEML2` computes six-component Mandel strain with shape
     `[nelem, nqp, 6]` from displacement gradients;
   - the first material-property implementation gathers a nine-component deformation gradient;
   - the pinned `KokkosQuantityToNEML2` gathers scalar/vector FE variables, not material properties.
4. Compare `MOOSEQuantityToNEML2` bulk and `executeOnInterface()` semantics against the batch-index API.
5. Confirm whether MOOSE registration permits one externally named object with two internal dispatch
   implementations. If not, use two registered concrete classes sharing one implementation base.

**Verify:** produce a mapping table from every pinned gatherer behavior to its destination, explicit
later phase, or documented unsupported case. No caller, shape, state, or lifecycle behavior may be
silently dropped.

### Phase 2: add reusable Kokkos interface user-object dispatch

Create the smallest general `Moose::Kokkos::InterfaceUserObject` infrastructure needed for this work.
Model its lifecycle on `KokkosElementUserObject`/`KokkosSideUserObject`, and model its interface-face
handling on `KokkosInterfaceMaterial`:

1. Require a boundary restriction and reject incompatible block-only semantics.
2. Install a Kokkos mesh-initialization hook.
3. Acquire the shared interface face cache and neighbor-QP map from `FEProblemBase`.
4. Reset dispatch state when the cache is reacquired.
5. Dispatch over cached interface faces.
6. Construct the primary and neighbor `Datum` objects using established interface helpers.
7. Expose the mapped neighbor quadrature point to the derived object's device execution method.
8. Add the relationship manager required for primary/neighbor data and ghosting.
9. Keep the base general to framework use; do not place NEML2-specific behavior in it.

Do not copy interface-material property computation into the user-object base. Reuse existing cache and
`Datum` facilities, and add only the execution contract a Kokkos interface user object requires.

**Verify:** add a small framework test object that writes or checks deterministic primary/neighbor
values entirely in device execution. Cover mapped neighbor QPs and a distributed interface so the test
would fail if neighbor access or ghosting were wrong.

### Phase 3: implement scheduling, device index maps, and the shared gatherer core

The batch-index generator and gatherers must use different execution groups. The Action sets
`NEML2BatchIndexGenerator` to `execution_order_group = -1`, Kokkos gatherers to group `0`, and the host
executor to group `0`. This makes the host generator complete before gathering; within group `0`,
`FEProblemBase::computeUserObjects()` runs Kokkos gatherers before the host executor. Preserve the
executor's explicit gatherer dependencies as an additional ordering check. Do not place the host
generator in group `0`, where Kokkos objects would run first.

Add the common implementation for `KokkosQuantityToNEML2`:

1. Implement the existing `MOOSEToNEML2` gatherer contract and resolve the current-state Kokkos
   rank-two material property.
2. After the host generator finalizes in its earlier execution group, copy its authoritative bulk or
   side offset maps into compact Kokkos arrays. Device kernels may read only these arrays; they must
   not reconstruct ordering from element IDs or face traversal.
3. Rebuild and recopy offset arrays whenever the generator's map generation changes. Add a monotonic
   generation counter updated after `initialize()`/`finalize()` rebuilds, including mesh changes; the
   gatherer records the generation used to size its buffer and resets stale dispatcher views after
   any reallocation.
4. Allocate a writable Kokkos device buffer for each gather pass and keep it alive through immediate
   NEML2 evaluation. Define the nine-component row-major rank-two layout consistently with existing
   NEML2 tensor conventions.
5. Provide device-callable writes into a destination batch entry.
6. Fence after Kokkos gathering and before Torch observes the allocation.
7. Wrap device storage with `at::from_blob` using the validated device and dtype handling from
   `KokkosSmallStrainToNEML2`.
8. Separate ephemeral input views from retained history. The executor may consume the zero-copy view
   for the current evaluation, but `advanceState()` must clone/copy the accepted value into
   executor-owned storage before placing it in pending history. Buffer rotation is an acceptable
   alternative only if an accepted buffer becomes immutable and remains owned until no history tensor
   aliases it. Never retain a mutable gather-buffer view as history.
9. Preserve repeated nonlinear-evaluation and cutback behavior: rejected iterations may overwrite only
   the current gather buffer, never an accepted history snapshot.

No host mirror or `tensor.to(cuda)` staging step is permitted on the CUDA input path. A device-to-device
snapshot made only when accepting state/history is permitted and required when using a reusable
`from_blob` buffer.

**Verify:** focused CPU tests check tensor shape, all nine components, deterministic offsets, and the
very first execution. Rebuild the mesh and verify map generation, buffer sizing, and offsets are
refreshed. A history test records distinct values across nonlinear iterations, accepts a timestep,
forces a cutback/retry, and proves old/accepted inputs remain unchanged after subsequent gathers.

### Phase 4: add bulk element gathering

Implement bulk dispatch using `Moose::Kokkos::ElementUserObject` or the established equivalent:

1. For each element/QP, read the Kokkos material property in device code.
2. Query/use the existing bulk batch destination rather than append order.
3. Write all nine deformation-gradient components into the shared buffer.
4. Support current-state full rank-two material properties in this phase. Do not claim parity with
   the pinned variable-input gatherer or the small-strain gatherer.

**Verify:** add a bulk deformation-gradient regression that checks all nine asymmetric components,
first-execution ordering, repeated nonlinear evaluation, and state snapshot behavior. Keep the existing
small-strain regression on its old gatherer until Phase 7 supplies its distinct six-component contract.

### Phase 5: add interface primary/neighbor gathering

Implement interface dispatch using the new `Moose::Kokkos::InterfaceUserObject`:

1. Traverse cached faces in device code.
2. For each primary QP, obtain the mapped neighbor QP.
3. In the first implementation, read a block material property from `FACE_MATERIAL_DATA` at the
   primary QP and `NEIGHBOR_MATERIAL_DATA` at the mapped neighbor QP.
4. Write both values into the respective side offsets supplied by `NEML2BatchIndexGenerator`.
5. Preserve the block-material branch of host `MOOSEQuantityToNEML2::executeOnInterface()`: one
   gatherer produces one tensor containing both sides.
6. Reject a property sourced from true `INTERFACE_MATERIAL_DATA` with a setup error. That storage has
   one interface value in the host implementation; do not invent a duplicate-primary/neighbor mapping.
7. Ensure every output slot has one writer. Do not rely on face iteration order or an atomic append
   counter.

**Verify:** add an asymmetric 3D fixture whose primary and neighbor tensors have distinct values in all
nine components. Check every gathered component and destination index. Run in serial and with two MPI
ranks using distributed mesh; exercise mapped neighbor-QP ordering rather than only identity maps.

### Phase 6: integrate `NEML2Action` with explicit input selection

Add `input_backend = moose kokkos` with default `moose`. This is independent of `output_backend`:
MOOSE inputs with Kokkos outputs and Kokkos inputs with MOOSE outputs remain valid combinations. Because the
Action creates gatherers before materials exist, it must not inspect a property name and claim to have
detected residency.

| `input_backend` and domain | Gatherer |
|---|---|
| `moose`, bulk or interface | `MOOSEQuantityToNEML2` |
| `kokkos`, bulk | `KokkosQuantityToNEML2` |
| `kokkos`, `interface_only` | `KokkosInterfaceQuantityToNEML2` |

1. Select solely from explicit `input_backend`, mapped MOOSE quantity type, and domain. Validate the
   selected property against the expected Kokkos storage when objects are constructed/setup.
2. In the first implementation, permit `input_backend = kokkos` only for current-state full rank-two
   material inputs. Emit setup errors for variables, old/older material states, unsupported ranks, and
   true interface-material storage; do not silently fall back to host staging.
3. Pass block restrictions to bulk dispatch and interface boundary restrictions to interface dispatch.
4. Assign the host index generator, Kokkos gatherers, and executor to the execution groups specified in
   Phase 3.
5. Preserve NEML2 variable naming and Action-mode automatic SCZM setup, including PK2 stress/tangent
   conversion.

**Verify:** Action-created objects for MOOSE/Kokkos bulk and `interface_only` configurations have the
expected registered types, restrictions, execution groups, and independent input/output backend
combinations. Invalid Kokkos source modes fail during setup. Existing host NEML2 tests remain unchanged.

### Phase 7: close small-strain and pinned-variable migration gaps

This phase is required before retirement but is not a prerequisite for the SCZM deformation-gradient
path.

1. Add or identify a Kokkos material that computes infinitesimal strain from displacement gradients
   with the same mathematics as `KokkosSmallStrainToNEML2`.
2. Extend the unified material-property family with symmetric rank-two Mandel packing and preserve the
   exact `[nelem, nqp, 6]` tensor contract.
3. Migrate each small-strain caller only after old/new gathered tensors and model outputs agree.
4. Decide separately whether the pinned scalar/vector variable-input `KokkosQuantityToNEML2` behavior
   is still required. Port its current/old/older state and dependency semantics or document and test an
   intentional unsupported case; material gathering does not automatically replace it.
5. Remove `KokkosSmallStrainToNEML2` only after call-site inspection confirms no users remain and its
   regressions run through the unified family. Remove only references made obsolete by this migration.

**Verify:** all six Mandel components, shape, repeated execution, and state behavior match the pinned
small-strain implementation. The migration table from Phase 1 has no unresolved rows before retirement.

### Phase 8: add device deformation-gradient production and SCZM end-to-end regression

The current fixture's `ComputeLagrangianStrain` is a host material and cannot feed the intended direct
Kokkos path. Add a Kokkos deformation-gradient material in the solid-mechanics Kokkos material area
(or extend an existing equivalent if found) before switching the gatherer:

1. Compute `F = I + grad(u)` in device code as a full rank-two Kokkos material property.
2. Make the property available as block material data so interface reinit provides distinct
   `FACE_MATERIAL_DATA` and mapped `NEIGHBOR_MATERIAL_DATA`; do not store it as a one-valued interface
   material property.
3. Match `ComputeLagrangianStrain` kinematics used by the fixture, including displacement component
   ordering, dimensional padding, and the unstabilized total deformation gradient. If the fixture
   needs F-bar, incremental kinematics, displaced-mesh behavior, or coordinate-system terms, stop and
   implement/validate those semantics rather than silently simplifying them.
4. Add focused bulk and two-sided interface checks with asymmetric gradients, verifying every one of
   the nine components on primary and neighbor sides and mapped neighbor-QP selection.
5. Replace the fixture's host producer, then route both sides through the new interface gatherer.
6. Keep NEML2 PK2 stress and `dPK2/dF` outputs, automatic primary/neighbor PK2-to-PK1 conversion, the
   device oracle checking all 9 PK1 and 81 tangent components per side, `PetscJacobianTester`, and
   Action-mode coverage.

**Verify:** kinematics tests and focused SCZM tests pass in serial and at two MPI ranks with distributed
mesh. Existing oracle and Jacobian tolerances must not be loosened to accommodate the transfer change.

### Phase 9: CUDA qualification

Run only in a CUDA-enabled MOOSE/Kokkos/NEML2 environment:

1. Compile the framework, solid mechanics, NEML2 integration, and shifted-boundary module with CUDA.
2. Confirm the gather kernels compile as device code and do not invoke host-only APIs.
3. Confirm `at::from_blob` reports CUDA storage on the intended device.
4. Confirm the wrapped tensor's data pointer corresponds to the Kokkos allocation.
5. Execute both a bulk NEML2 case and the interface SCZM case.
6. Exercise repeated nonlinear evaluations and multiple timesteps to test synchronization and lifetime.
7. Use tooling available in that environment to detect an accidental device-to-host-to-device copy.
8. Run the focused CUDA tests more than once to expose stream or lifetime races.

**Acceptance:** only after these checks pass may the path be described as CUDA-qualified direct
Kokkos-to-Torch input transfer. Until then, describe it as implemented and CPU-mapping-tested with CUDA
qualification pending.

## Verification cadence

Treat each phase as one coherent reviewable slice. Do not build after every edit.

1. Inspect source and diff while implementing a slice.
2. Format changed C++ files when the slice is complete.
3. Run the narrowest relevant compile.
4. Run the smallest focused test.
5. Expand to serial/distributed SCZM coverage at integration milestones.
6. Run CUDA checks only in the required NVIDIA environment.

For the local macOS workflow, use `./scripts/mac/moose.sh`. Conda environment `moose` is already
established for this repository. CPU tests on macOS must not be presented as GPU evidence.

## Expected files and blast radius

Likely framework additions or changes:

- `framework/include/kokkos/userobjects/KokkosInterfaceUserObject.h`
- `framework/src/kokkos/userobjects/KokkosInterfaceUserObject.K`
- `framework/include/neml2/userobjects/KokkosQuantityToNEML2.h`
- corresponding Kokkos/NEML2 source implementation
- `framework/src/neml2/actions/NEML2Action.C`
- `framework/include/neml2/userobjects/NEML2BatchIndexGenerator.h`
- `framework/src/neml2/userobjects/NEML2BatchIndexGenerator.C`
- `framework/include/neml2/userobjects/NEML2ModelExecutor.h`
- `framework/src/neml2/userobjects/NEML2ModelExecutor.C`
- a solid-mechanics Kokkos deformation-gradient material and its focused kinematics tests
- Kokkos interface-user-object and NEML2 gatherer tests

Existing contracts to inspect and preserve:

- `framework/include/kokkos/userobjects/KokkosUserObject.h`
- `framework/include/kokkos/userobjects/KokkosElementUserObject.h`
- `framework/include/kokkos/userobjects/KokkosSideUserObject.h`
- `framework/include/kokkos/materials/KokkosInterfaceMaterial.h`
- `framework/src/kokkos/materials/KokkosInterfaceMaterial.K`
- `framework/include/neml2/userobjects/MOOSEToNEML2.h`
- `framework/include/neml2/userobjects/MOOSEQuantityToNEML2.h`
- `framework/src/neml2/userobjects/MOOSEQuantityToNEML2.C`
- `framework/include/neml2/userobjects/NEML2BatchIndexGenerator.h`
- `framework/src/neml2/userobjects/NEML2BatchIndexGenerator.C`
- `framework/include/neml2/materials/NEML2ToKokkosMaterialProperty.h`
- `framework/src/kokkos/materials/NEML2ToKokkosMaterialProperty.K`
- pinned branch implementations of `KokkosSmallStrainToNEML2` and variable-input
  `KokkosQuantityToNEML2` at `258b2838722286e21d32e2e59929591f1391a818`

SCZM regression files to preserve or extend:

- `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/sczm_pk2_kokkos.i`
- `modules/shifted_boundary_method/test/include/kokkos/materials/KokkosPK2ToPK1InterfaceOracle.h`
- `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/tests`

## Completed prerequisites

The following work is already complete and is not to be reimplemented:

- Automatic SCZM Kokkos interface PK2-to-PK1 conversion:
  `2e6eb25084ad64ff4ef8ea983d43b48d4d287725`.
- Full-component primary/neighbor PK1/tangent oracle, 3D PK2 fixtures, Jacobian test, and Action-mode
  coverage: `75b673866b`.
- CPU verification at that checkpoint: 67 serial SCZM tests passed with 1 skipped; 28 two-rank
  distributed tests passed.

These commits establish downstream correctness coverage. This plan changes how NEML2 inputs reach the
model, not the PK2-to-PK1 mathematics.

## Stop conditions and design review gates

Stop and discuss before proceeding if any of these becomes necessary:

- changing a public NEML2 batch ordering or state-remapping contract;
- changing existing output retriever primary/neighbor semantics;
- adding host staging to make the CUDA path function;
- modifying Torch FEM classes to support material-property transfer;
- introducing separate primary and neighbor input gatherers;
- widening the first implementation beyond current-state full rank-two block-material properties;
- defining semantics for true `INTERFACE_MATERIAL_DATA` without a separate design decision;
- restructuring Kokkos user-object execution beyond the new reusable interface dispatch base and the
  explicit execution-group ordering required here.

## Definition of done

- Current-state full rank-two bulk and two-sided block-material interface inputs use the
  `KokkosQuantityToNEML2` family; unsupported storage/types fail explicitly.
- Interface primary and neighbor values are gathered by one object into existing side batch offsets.
- The index generator runs before Kokkos gathering, and first execution plus mesh-rebuild tests prove
  device offset maps are current.
- Accepted history owns immutable snapshots across nonlinear iterations and cutbacks.
- `input_backend` is explicit and independent of `output_backend`.
- A Kokkos deformation-gradient producer supplies both interface sides and passes all-component
  kinematics checks.
- Bulk callers no longer depend on `KokkosSmallStrainToNEML2` only after six-component Mandel parity;
  until then that gatherer remains supported rather than being incorrectly replaced.
- CUDA implementation contains no host staging between Kokkos storage and the wrapped Torch tensor.
- CPU tests prove component mappings, batch ordering, repeated execution, Action selection, history,
  mesh rebuilding, and serial/distributed interface behavior.
- Existing SCZM PK2 conversion oracle and Jacobian tests pass unchanged in intent.
- CUDA compilation and bulk/interface execution pass before any CUDA qualification claim is made.
