# Design & Implementation Plan: GPU Porting of SCZM via MOOSE Kokkos Path

## 0. Entry Point — Start Here

This file is the **architecture**. It is not the source of truth for what is currently built,
tested, or dirty.

**Read [Implementation Handoff](implementation_handoff.md) first.** It carries:

- the commit-by-commit inventory of what exists (snapshot HEAD `23d8bc67cd`, branch `sczm_dev_gpu`);
- the per-file working-tree WIP inventory and which task owns which file;
- the numbered pitfall list (P1-P21) that every task below refers to;
- the exact build and TestHarness commands, and the environment gate that must be answered first;
- per-task acceptance criteria and the continuation log.

Rules for using this pair of documents:

1. **Where they disagree, the handoff wins.** Any completion statement below that is not backed by
   retained evidence in the handoff §4 is a historical claim, not an acceptance result.
2. **Implementation is not GPU verification.** No CUDA or XPU run has been performed. CPU Kokkos
   results (`--compute-device=cpu`) do not establish device compilation or GPU residency.
3. **Task C is verified but awaiting review/commit.** The direct old/older regression, inspected gold,
   deterministic rejection, negative controls, recovery/restep, and CPU parallel modes are recorded
   in handoff §4.3. Its three accepted steps place the forced rejection after recovery's PART1
   checkpoint, so no `TimeStepper` framework change is included. Its test material is
   `KokkosTractionSeparationLawInterfaceMaterial`, renamed during the slice because the previous name
   implied a generic stateful base material. This is CPU Kokkos evidence only; no CUDA/XPU run occurred.
4. **TestHarness mode flags are not separate tests.** `-p2`, `--n-threads`, `--distributed-mesh`,
   `--recover`, and `--restep` are applied to the same spec. Retained evidence does not certify a
   clean build of the snapshot HEAD; see handoff §4.

Snapshot: 2026-10-12, branch `sczm_dev_gpu`, HEAD `23d8bc67cd`.

## 1. Executive Summary & Core Architectural Principle

The objective of this work is to port the **Shifted Cohesive Zone Method (SCZM)** to GPU execution via the **MOOSE Kokkos backend**.

In an **end-to-end GPU nonlinear solver pipeline** (where volumetric element assembly, constitutive model updating, and linear/nonlinear solves are fully resident on the GPU), running SCZM on the CPU forces costly Host $\leftrightarrow$ Device synchronizations and data copies at every Newton iteration.

### Golden Architectural Rule
> **Build the missing generic MOOSE Kokkos two-sided FE interface infrastructure first.**
> SCZM must be implemented as a **thin physics layer** on top of `KokkosShiftedInterfaceKernel`, **not** as an ad-hoc, SCZM-specific GPU assembly hack.

---

## 2. Target Class Hierarchy & Device Execution Model

To avoid code duplication and support future interface/DG/SBM kernels on GPU, we establish a three-tier compile-time hierarchy:

```text
       KokkosInterfaceKernel               (Generic 2-sided FE interface infrastructure)
                │
                ▼
    KokkosShiftedInterfaceKernel           (Generic SBM / shifted-interface kinematics)
                │
                ▼
KokkosSCZMInterfaceKernelSmallStrain       (SCZM-specific traction-separation physics)
```

### 1. `KokkosInterfaceKernel` (MOOSE Framework Level)
Generic infrastructure for two-sided finite element interface evaluations:
- **Two-Sided Element Access**: Manages primary `elem` and `neighbor` pairs.
- **Dual `AssemblyDatum`**: Evaluates fields, gradients, test functions, and shape functions on both sides.
- **Physical Quadrature-Point Correspondence**: Abstraction ensuring quantities on `elem` and `neighbor` are evaluated at the identical physical interface point $\boldsymbol{x}_{\text{interface}}$.
- **Two-Sided Residual Assembly**: Dispatches and accumulates residual contributions for element degrees of freedom ($R_{\text{elem}}$) and neighbor degrees of freedom ($R_{\text{neighbor}}$).
- **Four Jacobian Blocks**: Generic assembly for:
  - $J_{ee}$: `elem` $\rightarrow$ `elem`
  - $J_{en}$: `elem` $\rightarrow$ `neighbor`
  - $J_{ne}$: `neighbor` $\rightarrow$ `elem`
  - $J_{nn}$: `neighbor` $\rightarrow$ `neighbor`
- **Atomic Assembly**: Standard atomic accumulation into existing `KokkosSystem` / PETSc GPU matrix and vector data structures.

### 2. `KokkosShiftedInterfaceKernel` (SBM Module Level)
Generic shifted boundary / interface method infrastructure:
- **Shift Vector**: Manages the surrogate-to-true distance vector $\boldsymbol{d}$ at each primary-side
  quadrature point of each surrogate face. **There is one vector per face QP, not one per side.**
  The host formulation evaluates both terms at a matched physical point using the *primary* side
  distance, so the neighbor side reuses the primary value; do not introduce a separately signed
  $\boldsymbol{d}^+$ without first reading the host source. `shift()` is only valid for the primary
  datum: the geometry offset table is populated for the primary element of each oriented face.
- **True Interface Normals & Geometric Correction**:
  - Manages true boundary normal $\boldsymbol{n}_{\text{true}}$.
  - Provides the same geometric/area correction used by the existing host shifted-interface formulation (trace host CPU implementation prior to coding to confirm exact orientation, absolute value, or measure correction; do not modify the formulation during GPU porting). The implemented correction is the **signed** product
    $\boldsymbol{n}_{\text{true}} \cdot \boldsymbol{n}_{\text{surrogate}}$; do not change it to an absolute value.
- **Shifted Kinematics (First-Order Shifted Values, No Gradient Taylor Correction)**:
  - First-order shifted value:
    $$u(\boldsymbol{x}_{\text{true}}) \approx u(\boldsymbol{x}_s) + \nabla u(\boldsymbol{x}_s) \cdot \boldsymbol{d}$$
  - Gradient evaluated directly at surrogate QP (no Hessian / second-order Taylor correction):
    $$\nabla u(\boldsymbol{x}_{\text{true}}) \approx \nabla u(\boldsymbol{x}_s)$$
  - Helpers:
    - `shiftedValue`: $u_{\text{shifted}} = u + \nabla u \cdot \boldsymbol{d}$
    - `shiftedTest`: $\phi_{\text{shifted}} = \phi + \nabla \phi \cdot \boldsymbol{d}$
    - `shiftedShape`: $\phi_{\text{shifted}} = \phi + \nabla \phi \cdot \boldsymbol{d}$
    - Gradient: directly use $\nabla u$ evaluated at surrogate QP. The `shiftedGradient()` helper is **deliberately omitted** to prevent confusion.
- **Scope Matrix**:
  | Quantity | Treatment | Notes |
  | :--- | :---: | :--- |
  | Value shift | ✓ First order | $u + \nabla u \cdot \boldsymbol{d}$ |
  | Test function shift | ✓ First order | $\phi + \nabla \phi \cdot \boldsymbol{d}$ |
  | Shape function shift | ✓ First order | $\phi + \nabla \phi \cdot \boldsymbol{d}$ |
  | Gradient shift | ✗ Omitted | Evaluated directly at surrogate QP |
  | Hessian | ✗ Omitted | No second-order terms in initial GPU port |

### 3. `KokkosSCZMInterfaceKernelSmallStrain` (Physics Layer)
Contains **only** SCZM-specific small-strain cohesive zone physics:
- Consumes global traction and its consistent shifted-jump tangent from Kokkos interface materials.
- The materials compute shifted jump, local/global rotation, cohesive law, and tangent. Do not move
  those producers into the kernel; the test-only linear derived kernel is not a production pattern.
- Computes residual and Jacobian formulas and delegates matrix block insertion to the base classes.

### Device Execution & Polymorphism Policy
- **No device-side virtual dispatch or runtime polymorphism**: Virtual functions (`virtual`, `override`, dynamic vtables) are strictly forbidden inside `KOKKOS_FUNCTION` device kernels.
- **Compile-time inheritance & inlining**: Normal C++ class inheritance is used. Device-callable methods (`computeQpResidual`, `computeQpJacobian`, etc.) must be non-virtual, static-dispatched, and marked `KOKKOS_INLINE_FUNCTION`.

---

## 3. Static Geometry Strategy: GPU-Resident Lookup Table

For static / non-moving interfaces, the geometric search is performed **only once on the CPU during initial setup**:

```text
BoundaryShortestDistanceToSurface / KD-tree search (CPU, t = 0)
                            ↓
Find closest point on true surface for each surrogate face QP
                            ↓
Compute d-, d+, and true interface normal n_true
                            ↓
Flatten into contiguous Kokkos Views
                            ↓
Kokkos::deep_copy once to Device
                            ↓
GPU-resident runtime data (no per-Newton Host ↔ Device geometry transfers)
```

### Runtime Lookup
During transient time steps and nonlinear Newton iterations, no KD-tree or geometric searches occur. The GPU kernel performs an $O(1)$ memory lookup, indexed by the primary datum only:

```cpp
// Conceptual form. The implemented accessors take the primary AssemblyDatum,
// because the per-(side, element) geometry offset table is only populated for
// the primary element of each oriented face record.
const Real3 d_elem = shift(elem_datum, qp);        // one vector per face QP
const Real3 n_true = trueNormal(elem_datum, qp);
```

> **Note on Terminology**: This architecture provides **GPU-resident runtime data with no per-Newton Host $\leftrightarrow$ Device geometry transfer**. (Avoid calling this "zero-copy", as memory is explicitly mirrored and resident in GPU High-Bandwidth Memory).

---

## 4. Interface Topology & Quadrature Discretization

### GPU Parallelization Model
Interface faces form the outer parallelization unit. The execution hierarchy is structured hierarchically:

```text
Kokkos::parallel_for (over interface faces)
    └── local / team parallelism (over test / trial DOFs)
            └── loop / parallel (over face quadrature points)
```

> **Important**: Do not interpret "one face = one work item" as a single CUDA thread doing all operations sequentially for the entire face. Use Kokkos TeamPolicy / hierarchical parallelism where appropriate to distribute test/trial DOFs and QPs across thread teams/warps.

### Interface Face Topology Record
Precompute an oriented interface-face list during host initialization to ensure each physical interface face is visited exactly once:

```cpp
struct InterfaceFaceRecord
{
  ContiguousElementID elem;
  unsigned short int side;
  ContiguousElementID neighbor;
  unsigned short int neighbor_side;
};
```

Only one orientation per physical interface face should be stored in
`Kokkos::View<InterfaceFaceRecord*>`. Preserve the boundary-selected primary side; do not
unconditionally filter by `elem.id < neighbor.id`, because a boundary marked on only one side
could then lose its only record. The current `(elem, side)` visited set does not deduplicate
opposite orientations when both sides are marked.

**Implemented decision (`76dc6cc212`):** setup rejects a physical interface face whose boundary is
marked from both orientations and tells the user to mark only one side. Silently selecting an
orientation would change the primary side, normal, and canonical interface-property owner. A
RunException regression covers this rule.

### Physical Quadrature-Point Correspondence
The fundamental mathematical requirement across the interface is:
> **Evaluate element and neighbor quantities at the same physical interface point $\boldsymbol{x}_{\text{interface}}$.**

- **Initial Conforming Mesh Fast Path**: For conforming meshes with matching face quadratures, a precomputed quadrature index permutation:
  ```cpp
  const unsigned int neighbor_qp = _qp_map(face_id, qp);
  ```
  maps the element face QP to the coincident neighbor face QP.
- **Framework Abstraction**: The framework design shall not make "QP index permutation" a permanent structural limitation; the interface abstraction is defined around **physical point evaluation** ($\boldsymbol{x}_{\text{elem}}(qp) = \boldsymbol{x}_{\text{neigh}}(qp')$).

---

## 5. Assembly Strategy: Generic Two-Element Matrix Accumulator

### No SCZM-Specific CRS Indexing
Do **not** build custom `face_to_crs_index(...)` lookup tables. Instead, extend MOOSE's existing `KokkosResidualObject` / `KokkosSystem` abstraction to allow specifying distinct row and column elements:

```cpp
KOKKOS_FUNCTION inline void
ResidualObject::accumulateTaggedInterfaceMatrix(const Real local_ke,
                                                const ContiguousElementID row_elem,
                                                const ContiguousElementID col_elem,
                                                const unsigned int i,
                                                const unsigned int j,
                                                const unsigned int ivar,
                                                const unsigned int jvar,
                                                const unsigned int comp = 0) const
{
  if (!local_ke)
    return;

  auto & sys = kokkosSystem(_kokkos_var.sys(comp));
  const auto row = sys.getElemLocalDofIndex(row_elem, i, ivar);
  const auto col = sys.getElemGlobalDofIndex(col_elem, j, jvar);

  for (unsigned int t = 0; t < _matrix_tags.size(); ++t)
  {
    const auto tag = _matrix_tags[t];
    if (sys.isMatrixTagActive(tag) && !sys.hasNodalBCMatrixTag(row, tag))
      ::Kokkos::atomic_add(&sys.getMatrixValue(row, col, tag), local_ke);
  }
}
```

### Clean Mapping of the Four Jacobian Blocks
```text
Jee (Element-Element):   row_elem = face.elem,     col_elem = face.elem
Jen (Element-Neighbor):  row_elem = face.elem,     col_elem = face.neighbor
Jne (Neighbor-Element):  row_elem = face.neighbor, col_elem = face.elem
Jnn (Neighbor-Neighbor): row_elem = face.neighbor, col_elem = face.neighbor
```

### Sparsity Pattern Preservation
- Reuse `ElementSideNeighborLayers` RelationshipManager on the Host:
  ```cpp
  params.addRelationshipManager("ElementSideNeighborLayers",
                                Moose::RelationshipManagerType::GEOMETRIC |
                                Moose::RelationshipManagerType::ALGEBRAIC |
                                Moose::RelationshipManagerType::COUPLING);
  ```
- This ensures libMesh's `DofMap` includes all cross-interface coupling entries $(elem, neighbor)$.
- `KokkosSystem::setupSparsity()` naturally allocates the device PETSc/Kokkos sparse matrix with required off-diagonal slots. **Do not independently construct a CRS graph.**

### Race Condition Policy
1. **Plan A (Official)**: Use backend-supported `Kokkos::atomic_add` on global matrix and vector values. Interface elements are sparse relative to the volume mesh. Profile contention later.
2. **Face Coloring Policy**: **Do not implement face coloring initially.** Follow the progression:
   $$\text{Correctness} \rightarrow \text{GPU-Resident Execution} \rightarrow \text{Atomic Assembly} \rightarrow \text{Profile} \rightarrow \text{Only add coloring if atomic contention is proven significant.}$$

---

## 6. Complete SCZM Runtime Execution Flow

Ownership matters more than the arithmetic here. **The jump, the rotation, and the cohesive law are
produced by interface *materials*; the kernel is a consumer.** The kernel does not compute the
displacement jump and does not own a cohesive law.

```text
       MATERIAL LAYER (InterfaceMaterial, owner rank, device-resident properties)
       elem Datum: u-, ∇u-      neighbor Datum: u+, ∇u+
                    \                 /
                     \               /
                      ▼             ▼
                  Shifted Kinematics  (u + ∇u · d, primary-side d for both terms)
                               ↓
                 jump_global = (u+ + ∇u+ · d) - (u- + ∇u- · d)
                               ↓
                 Q = computeReferenceRotation(true_normal, dim)
                               ↓
                 interface_displacement_jump = Qᵀ · jump_global
                               ↓
                 Cohesive law + global rotation  →  traction_global
                                                     dtraction_djump_global

       KERNEL LAYER (KokkosSCZMInterfaceKernelSmallStrain, pure consumer)
                               ↓
        sign · test · areaCorrection · traction_global(_component)
        test · areaCorrection · dtraction_djump_global · shiftedShape(d)
                               ↓
                 Re, Rn, Jee, Jen, Jne, Jnn
                               ↓
              Kokkos Atomic Assembly
             (accumulateTaggedInterfaceResidual / Matrix)
                               ↓
                    PETSc GPU Matrix
```

The kernel reads both properties through the **primary** datum, so the primary and neighbor
residual rows consume one canonical property instance rather than a synchronized mirror.

---

## 7. Implementation Phases & Step-by-Step Verifiable Goals

```text
Phase 1:  [Host Setup & GPU Shift Cache]       -> verify: Device View values match CPU KD-tree queries within machine precision
Phase 2:  [KokkosInterfaceKernel Base]         -> verify: Two-sided AssemblyDatum & physical QP correspondence evaluate correctly
Phase 3:  [Generic Two-Element Assembler]      -> verify: Jee, Jen, Jne, Jnn assemble to correct (row, col) slots without memory faults
Phase 4:  [KokkosShiftedInterfaceKernel]       -> verify: Shifted u_shifted and test variations match CPU SBM reference
Phase 5a: [SCZM Interface Kernel & Assembly]   -> verify: Property-driven residual and all interface Jacobian blocks linearize correctly
Phase 5b: [Generic Kokkos InterfaceMaterial]   -> verify: Two-sided interface properties, state, restart, and ownership work correctly
Phase 5c: [Production SCZM Property Chain]     -> verify: Shifted jump, rotations, traction, and tangent match host properties
Phase 5d: [Action Integration & Host Parity]   -> verify: Physics syntax selects Kokkos objects and full residual/Jacobian match host SCZM
Phase 6:  [End-to-End Nonlinear Verification] -> verify: Complete GPU simulation converges to same physical solution as CPU
```

### Detailed Phase Tasks

#### Phase 1: Host Preprocessing & GPU-Resident Geometry Cache
- Connect to `BoundaryShortestDistanceToSurface`.
- Query closest points and compute $\boldsymbol{d}^-$, $\boldsymbol{d}^+$, and $\boldsymbol{n}_{\text{true}}$ at each quadrature point of each surrogate face.
- Copy into contiguous device `Kokkos::View<Real*[][3]>`.

#### Phase 2: `KokkosInterfaceKernel` Framework Class
- Add `framework/include/kokkos/interfacekernels/KokkosInterfaceKernel.h`.
- Implement `InterfaceFaceRecord` generation with single-orientation filtering.
- Implement physical QP correspondence mapping.
- Set up dual `AssemblyDatum` accessors.

#### Phase 3: Generic Interface Matrix & Residual Accumulator
- Add `accumulateTaggedInterfaceResidual` and `accumulateTaggedInterfaceMatrix` to `KokkosResidualObject`.
- Ensure integration with matrix tags and Dirichlet BC masking.

#### Phase 4: `KokkosShiftedInterfaceKernel` Intermediate Base
- Cache static geometry on device from the distance user object, once per mesh lifecycle:
  - the primary-side distance vector $\boldsymbol{d}$ and true normal $\boldsymbol{n}_{\text{true}}$, one
    entry per interface face quadrature point;
  - the per-(side, element) offset table, populated for the **primary** element of each oriented
    face only.
- Implement first-order shifted kinematic helpers used by the kernel's assembly terms:
  - `shiftedValue(datum, d, qp)`: evaluates $u + \nabla u \cdot \boldsymbol{d}$
  - `shiftedTest(datum, d, i, qp)`: evaluates $\phi + \nabla \phi \cdot \boldsymbol{d}$
  - `shiftedShape(datum, d, j, qp)`: evaluates $\phi + \nabla \phi \cdot \boldsymbol{d}$
  - Gradient: directly evaluate $\nabla u$ at the surrogate QP. (Omit a `shiftedGradient` helper).
  - No second-order Taylor terms or Hessian in the initial implementation.
- This base deliberately does **not** compute the displacement jump or a cohesive law. Those are
  material responsibilities (see Phase 5c and §9.3). Its only SCZM-adjacent term is the geometric
  area correction, which must match the host CPU SCZM implementation exactly (trace the CPU
  implementation prior to coding).

#### Phase 5a: `KokkosSCZMInterfaceKernelSmallStrain` Consumer and Assembly
- Consume device-resident `traction_global` and `dtraction_djump_global` properties.
- Assemble residual vectors $R_{\text{elem}}$ and $R_{\text{neighbor}}$.
- Assemble same-component and cross-component blocks $J_{ee}$, $J_{en}$, $J_{ne}$, and $J_{nn}$.

#### Phase 5b: Generic Kokkos `InterfaceMaterial`
- [x] Define interface-face/QP property indexing and distributed ownership before implementing storage.
- [x] Execute materials with simultaneous primary and neighbor data and mapped physical QPs.
- [x] Implement stateful hooks and reuse the generic boundary-property lifecycle and serialization.
- [x] Add current/old checkpoint and restart tests; retained CPU serial results show these passing.
- [x] Directly distinguish the older accepted state from old and prove that a forced rejected trial
      does not enter old/older accepted history after retry. The review slice requests older state,
      exposes separate old/older interface-consumed fields, compares exactly one failed step, and has
      both required negative controls. It remains uncommitted pending review; see handoff §4.3.
- [x] Re-record CPU MPI, thread, recover, restep, distributed, and distributed-recover results against
      the current source plus Task C dirty paths. These are not CUDA/XPU results; see handoff §4.3.

#### Phase 5c: Production SCZM Constitutive Property Chain
- [x] Compute the shifted displacement jump and local/global rotations in interface materials.
- [x] Evaluate the production pure-elastic cohesive traction law and consistent tangent on device.
- [x] Produce device-resident `traction_global` and `dtraction_djump_global` properties.
- [ ] Rebuild at clean HEAD and compare each property against the host oracle across dimensions,
      orientations, mapped QPs, and base names; the current composed Jacobian test proves
      self-consistency, not full host parity.
- [ ] Add/verify source documentation and SQA links for the new production objects.

#### Phase 5d: Action Integration and Host Formulation Parity
- Make `ShiftedCohesiveZoneAction` select a complete, consistent Kokkos object pipeline.
- Implement directional correction and its consistent bulk-stress tangent.
- Compare residuals and assembled Jacobians directly with the host SCZM formulation.

---

## 8. Verification & Acceptance Criteria

Because floating-point atomic additions on GPU are non-associative and may execute in different order from CPU serial/thread accumulation, **bitwise identity and identical Newton iteration counts are NOT required**.

Verification will be assessed against the following standards:

1. **Geometry & Physical QP Mapping**:
   - $\boldsymbol{d}$, $\boldsymbol{n}_{\text{true}}$, and physical quadrature coordinates match CPU preprocessing within target precision (e.g. target $|\Delta| \lesssim 10^{-14}$ in double precision).
2. **Residual Evaluation**:
   - Evaluated on identical solution states: residual $L_2$ norm difference against CPU gold satisfies tight tolerance ($|R_{\text{gpu}} - R_{\text{cpu}}| / |R_{\text{cpu}}| < 10^{-10}$).
3. **Jacobian Evaluation**:
   - Assembled system matrix compared directly against CPU matrix and validated using MOOSE's internal finite-difference/Jacobian verification (`-snes_test_jacobian` / `PetscCheckJacobian`).
4. **Final Converged Solution**:
   - Solution fields ($u_x, u_y, u_z$) after full nonlinear convergence match CPU gold solutions within acceptable numerical discretization error tolerance.
5. **Newton Convergence Behavior**:
   - Expected nonlinear convergence rate is demonstrated on GPU, with comparable overall convergence behavior and iteration counts (minor variations due to backend-specific FP reduction ordering are acceptable).

---

## 9. Current Implementation Status and Missing Pieces

This section records the architecture and implementation history. For current acceptance status use
the 2026-10-12 handoff linked above; all-mode test claims here are not independently re-established.
It reflects the generic interface infrastructure, shifted-interface kernel, and the production
pure-elastic SCZM material chain committed through `a52971472d`. The existing documentation targets
for the completed path are the
[SCZM Physics syntax page](../../modules/shifted_boundary_method/doc/content/syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md)
and the
[`ShiftedCohesiveZoneAction` source page](../../modules/shifted_boundary_method/doc/content/source/actions/ShiftedCohesiveZoneAction.md).
Do not use `KokkosInterfaceKernel.md` as a design reference: no such document currently exists.

### 9.1 Implemented and Focused-Tested

- Generic oriented two-sided Kokkos interface-face caching, neighbor metadata, physical-QP
  permutation, and dual `AssemblyDatum` execution.
- Element and neighbor residual rows and all four same-component interface Jacobian blocks
  $J_{ee}$, $J_{en}$, $J_{ne}$, and $J_{nn}$.
- Generic cross-component off-diagonal Jacobian dispatch using each row variable's coupling graph,
  including variables active on only one interface subdomain.
- Explicit row-variable interface residual and matrix accumulation through the existing tagged
  Kokkos assembly path.
- Generic shifted-interface geometry caching from `BoundaryShortestDistanceToSurface`, first-order
  shifted value/shape helpers, and signed area correction.
- Thin `KokkosSCZMInterfaceKernelSmallStrain` consumption of device-resident dimensional properties:
  `MaterialProperty<Real, 1>` for `traction_global` and `MaterialProperty<Real, 2>` for
  `dtraction_djump_global`.
- Shifted SCZM residual, same-component Jacobian, and cross-component Jacobian assembly.
- Generic `Moose::Kokkos::InterfaceMaterial` base and dispatchers, with a two-sided device hook
  receiving the oriented face record, primary and neighbor `AssemblyDatum`, primary `qp`, and mapped
  `neighbor_qp`.
- A generic two-sided test material plus a four-block interface kernel consuming its properties,
  with a CPU Kokkos Jacobian test that matches finite differences
  (`‖J−Jfd‖/‖J‖ = 2.04131e-09`, serial CPU). Earlier `-p2 --distributed-mesh` runs were reported but
  have not been re-recorded against the current source revision.
- Explicit known-neighbor `MeshDatum`, `Datum`, and `AssemblyDatum` constructors that let interface
  material and interface kernels skip the side topology lookup, which is unfilled for ghost elements.
- `makePrimaryInterfaceDatum()`/`makeNeighborInterfaceDatum()`, the shared device-callable helpers that
  own the primary/neighbor orientation of an `InterfaceFaceRecord` for both the material and kernel
  paths.
- A traction-separation-law interface test material (`KokkosTractionSeparationLawInterfaceMaterial`)
  plus checkpoint and restart tests, which show that the generic boundary material property lifecycle
  stores and reloads an interface property's current and old state. Task C adds direct older-state
  observation and a rejected-trial regression; see handoff §4.3.
- A production Kokkos SCZM pure-elastic property chain: shared surrogate geometry,
  `KokkosSCZMComputeDisplacementJumpSmallStrain`, `KokkosPureElasticTractionSeparation`, and
  `KokkosCZMComputeGlobalTractionSmallStrain`. `sczm_linear.i` composes these with the production
  kernel, and retained serial CPU finite-difference Jacobian evidence shows self-consistency.

These tests prove CPU interface assembly and linearization. They do **not** yet prove direct
property-by-property host parity, direct older/rejected-step behaviour, Action-created object graphs,
or end-to-end GPU residency.

### 9.2 Generic Two-Sided Kokkos Interface Materials (Implemented)

The blocker was that `Moose::Kokkos::Material` executes element, side, or neighbor data one side at a
time, while the production host SCZM pipeline is based on `InterfaceMaterial` and simultaneously
accesses primary and neighbor displacement values and gradients. A constant boundary material or a
derived test kernel is not a production substitute.

The contract below was resolved before coding and is now implemented as
`Moose::Kokkos::InterfaceMaterial`. The contract resolves:

- the face ID is stable across residual/Jacobian evaluations because material and kernel share one
  cache, rebuilt only on Kokkos mesh re-initialization;
- the rank owning the primary element of the oriented face record initializes and updates the face;
- ghost ranks read only, and never mirror or update, another rank's face property;
- QP addressing follows the assembly's per-face offsets, described below;
- current, old, and older state use the flattened face/QP layout in the existing Kokkos property
  `shift()`/`copy()` lifecycle, with rejected steps following the framework material-state restore
  path; and
- both residual rows read one canonical property instance, never a synchronized mirror.

#### Property Addressing (Correction)

Interface properties are **not** stored or indexed by an interface face ID. They use the ordinary
Kokkos boundary material property model unchanged:

- `MaterialPropertyValueBase` resolves an index as `datum.propertyIdx(constant_option, qp)` into
  `property._data[datum.subdomain()]` — i.e. **per-subdomain** storage
  (`framework/include/kokkos/materials/KokkosMaterialPropertyValue.h`);
- with `constant_on = NONE`, `Datum::propertyIdx()` returns `_qp_offset + qp`, and for a side datum
  `_qp_offset = Assembly::getQpFaceOffset(elem, side)`
  (`framework/include/kokkos/base/KokkosDatum.h`);
- `getElemFacePropertyIndex(elem, side)` is the **element-constant side** index, i.e. what
  `propertyIdx` returns for `constant_on = ELEMENT`. It is unrelated to `qpOffset()` and `qp` must
  never be added to it. `InterfaceMaterial` rejects every constant mode other than `NONE`
  (commit `eb401438c0`), so the `ELEMENT` branch is unreachable for interface materials today.

What is interface-specific is therefore **ownership and correspondence, not layout**: the
`InterfaceFaceCache` supplies the oriented face list and the neighbor QP map, the primary-element
owner is the single writer, and the kernel reads both rows from the primary datum
(`_traction_global(elem_datum, qp)`). There is no separate "per interface face" storage to keep in
sync and no per-face index to serialize.

Use the existing property accessors rather than reconstructing this indexing in a new material.

#### Selected Interface-Property Storage Contract

The first implementation shall use one canonical property instance owned by the rank that owns the
primary element in the oriented interface-face record. This matches the existing interface-kernel
execution rule: only the primary-element owner adds the face to its local cache, and that rank
assembles both the primary and neighbor residual rows. A ghost rank may hold the neighbor element and
solution data needed by another owned face, but it shall neither allocate a mirror of that face's
interface property nor update its state. No post-material ghost-property synchronization is therefore
required for this execution model.

Use two distinct identifiers for each face:

- `InterfaceFaceKey = (primary_elem_global_id, primary_side)` is the stable host identity, as opposed
  to the compact index below. It is **not implemented**; the code carries only `InterfaceFaceRecord`.
  It is recorded here as the identity to introduce if a future rewrite needs to recognize a face
  across a mesh rebuild, or to detect duplicate faces while building the cache. Boundary ID is
  metadata/restriction information and would not be part of the identity, because one geometric side
  may carry multiple boundary IDs.
- `interface_face_id` is a rank-local compact index into the device face cache. It may change whenever
  the cache is rebuilt and shall never be serialized or used to match state across repartitioning.
  It is a **cache lookup key only** — it is not a material property index (see
  *Property Addressing (Correction)* above).

The reusable cache owns the oriented `InterfaceFaceRecord` array and the mapped-neighbor-QP array. It is
shared by interface materials and interface kernels for the same boundary/restriction set, so a local
face index identifies the same face for both. Materials and kernels shall not build independently
ordered face lists and infer that equal local indices identify the same face.

Current, old, and older arrays use the same flattened layout and participate in the existing Kokkos
property `shift()`/`copy()` lifecycle. Initialization and nonlinear updates occur only on the owner.
Rejected-step behavior follows the framework's existing material-state restore path. Restart and
recovery need no interface-specific machinery: `InterfaceMaterial` properties live in
`_kokkos_bnd_material_props`, which is a `declareRestartableDataWithContext` member of `FEProblemBase`,
so the generic `Moose::Kokkos::MaterialPropertyStorage` `dataStore`/`dataLoad` already serialize every
stateful property, and `shift()`/`copy()`/stateful initialization are already wired for that storage.
Adaptivity remains unsupported by the Kokkos backend; interface-state projection must be designed
before that backend restriction is removed rather than treating parent and child compact IDs as
persistent.

State is not migrated by key across a repartition. `MooseMesh::init` deliberately disables both
partitioning and renumbering while recovering, because partitioners are not idempotent and some
recovery data requires the partitioning to match; MOOSE therefore never repartitions on restart or
recovery, and no keyed state transfer is required or exercisable.

Status of the gate:

1. **Implemented; retained evidence does not certify HEAD.** The shared cache can cross an MPI
   partition, and `KokkosLinearInterfaceMaterial::checkGhostNeighborCoverage()` asserts that a
   distributed run evaluates a face whose neighbor element is a one-layer ghost. A retained
   two-rank distributed CPU run exists for `regressions/neighbor_material_ghost.i`, but it predates a
   clean build of snapshot HEAD; see handoff §3/§4. It does **not** directly cover ordinary
   neighbor-material dispatch.
2. **Implemented; serial CPU verified only.** `KokkosLinearInterfaceMaterial` writes `traction`
   once at the primary-side property index and `KokkosLinearInterfaceDiffusion` assembles both
   residual rows from that single value, with the Jacobian matching finite differences
   (`‖J−Jfd‖/‖J‖ = 2.04131e-09`). No ghost-side mirror exists in the base class, so there is
   nothing to update on a ghost rank.
3. **Implemented; CPU Kokkos verified in all harness modes (no CUDA/XPU).**
   `KokkosTractionSeparationLawInterfaceMaterial` (renamed from `KokkosStatefulInterfaceMaterial`
   in Task C; the old name implied a generic stateful base material while the class implements a
   concrete traction-separation law) declares `traction` as a stateful property with old and older
   states, overrides `initQpStatefulProperties`, and accumulates each accepted step's increment into
   the previous traction, so the carried state is observable in the solution. The
   `interfacekernels.stateful` tests write a checkpoint and restart from it. Serial CPU, `--recover`,
   `--restep`, `--n-threads=2`, `-p2`, `-p2 --distributed-mesh`, and `-p2 --distributed-mesh
   --recover` were re-recorded against the current source in handoff §4.3.
4. Not applicable. See the repartition note above; MOOSE does not repartition on restart or recovery,
   so there is no partition change for state to be migrated across.

This contract deliberately does not support material execution on ghost-owned mirrors. If a future
consumer requires property reads on both ranks, add an explicit owner-to-ghost synchronization phase;
do not weaken the single-writer rule or create implicit duplicate state.

#### Neighbor Datum Construction on Ghost Elements

`MeshDatum` populates `_neighbor` by a side topology lookup of the mesh, and
`Mesh::_elem_neighbor` is filled only for active local elements. Constructing a neighbor-side datum
for an interface face whose neighbor element is a one-layer ghost therefore read uninitialized
memory, which crashed with `SIGBUS` under `-p2 --distributed-mesh`. The lookup is not needed for
any interface consumer, since the oriented face record already knows both sides.

The cache orients each face, so `InterfaceFaceRecord` fixes `neighbor_side` from
`face.neighbor->which_neighbor_am_i(face.elem)` and the pair is mutually consistent. `MeshDatum`,
`Datum`, and `AssemblyDatum` therefore take an optional known-neighbor element, and both
existing lookup-based constructors are retained unchanged so that non-interface callers, including
`FVDatum` consumers of `neighbor()`, keep their current semantics.

The datum classes stay generic and know nothing about interface faces. The orientation lives only in
the device-callable free functions `makePrimaryInterfaceDatum()` and
`makeNeighborInterfaceDatum()` in `KokkosInterfaceDatum.h`, which are templated on the datum type
and take the trailing variable arguments. Interface materials and interface kernels call those
instead of naming face fields themselves, so no caller can pass an inverted or omitted counterpart
element.

The facility is implemented as a generic framework base, not an SCZM-only callback:

1. Done. `Moose::Kokkos::InterfaceMaterial` and its `InterfaceInit`/`InterfaceCompute` dispatchers
   now pass the oriented face, primary and neighbor `AssemblyDatum`, primary `qp`, and mapped
   `neighbor_qp`. Commit `a52971472d` closed the former gradient-access gap for the production shifted
   jump material.
2. Done. The material acquires the `KokkosInterfaceKernelBase` oriented face cache through
   `FEProblemBase::getKokkosInterfaceFaceCache()` and holds shallow handles to its arrays, rather than
   building a second oriented face list.
3. Done for the residual and Jacobian ordering that Kokkos materials already provide.
4. Done. Interface properties are written at the assembly's primary-side QP property index and the
   kernel reads them through the primary datum, so a property written once on the primary side is
   read consistently by both residual rows. There is no separate per-interface-face storage (see
   *Property Addressing (Correction)*).
5. Done. Stateful initialization hooks and the old/older `shift()`/`copy()` lifecycle are in place,
   and checkpoint store and load come from the generic boundary material property storage as noted
   above, so no interface-specific restart code exists or is needed. Direct observation of *older*
   state and of a forced rejected step is still open (handoff task C).
6. Done. Boundary restrictions are required and block restrictions rejected, `base_name` is preserved
   through the existing `Material` base, and properties are declared with the standard Kokkos
   dependency checks.
7. **Serial CPU verified; other modes not re-recorded.** The Jacobian test and the stateful
   checkpoint/restart tests pass serially on CPU. Earlier runs under `--n-threads=4`, `-p2`,
   `-p2 --distributed-mesh`, `--recover`, `--restep`, and `-p2 --distributed-mesh --recover` were
   reported but have not been re-recorded against the current source revision. Re-run them on the
   same specs before citing them.

Success criterion status: a test material computes a property from both side values on device and an
interface kernel consumes it, with the Jacobian matching finite differences without any host-side
per-Newton property updates or copies. A second test material carries a stateful interface property
across a checkpoint write and a restart, which closes the restart half of the criterion.

### 9.3 Production Kokkos SCZM Property Pipeline — Implemented, Verification Incomplete

Commit `a52971472d` ports the initial production pure-elastic property chain rather than embedding a
traction law in `KokkosSCZMInterfaceKernelSmallStrain`. The implemented contract is:

1. `KokkosSCZMComputeDisplacementJumpSmallStrain` implements the shifted jump and rotation stage.
2. It computes the global shifted jump with the host sign convention:
   $[[\boldsymbol{u}]] = \boldsymbol{u}^{+}_{\mathrm{shifted}} -
   \boldsymbol{u}^{-}_{\mathrm{shifted}}$.
3. It computes the true-normal reference rotation and produces `displacement_jump_global`,
   `interface_displacement_jump`, and `czm_total_rotation` in device-safe representations.
4. `KokkosPureElasticTractionSeparation` consumes the local jump and produces local traction and its
   consistent tangent on device.
5. `KokkosCZMComputeGlobalTractionSmallStrain` rotates traction and tangent back to the global frame
   and produces `traction_global` and `dtraction_djump_global` for the Kokkos kernel.
6. **Still required:** compare each property directly against its host counterpart, cover dimensions,
   orientations, mapped QPs, base names, repeated evaluations, and verify clean-HEAD/device builds.
7. Define and test state evolution semantics for damage/history variables, including initialization,
   old/older state, rejected nonlinear steps, restart, recovery, and displaced/restarted runs.
8. If NEML2 is the intended constitutive producer, add an explicit device-resident interface batch
   path and property transfer contract. Merely selecting `device = cuda` in NEML2 is not sufficient
   if MOOSE interface inputs or outputs are staged through host material storage each Newton
   iteration.

Success criterion: the production Kokkos kernel consumes traction and tangent generated from the
current two-sided shifted displacement state entirely through device-resident interface properties.

### 9.4 Required: `ShiftedCohesiveZoneAction` Integration

The current Action selects host objects (`SCZMInterfaceKernelSmallStrain`,
`SCZMComputeDisplacementJumpSmallStrain`, and `CZMComputeGlobalTractionSmallStrain`). Add an explicit,
validated Kokkos selection path:

1. Decide the public selection mechanism by following the existing Kokkos execution configuration;
   do not introduce a separate SCZM-only backend switch if the application already exposes one.
2. Select `KokkosSCZMInterfaceKernelSmallStrain` and the Kokkos displacement-jump/global-traction
   materials together. Reject mixed host/Kokkos property pipelines with a clear parameter error.
3. Forward `boundary`, `displacements`, `component`, `base_name`, `sbm_distance_uo`, traction/tangent
   property names, and shifted options consistently.
4. Validate dimensional-property sizes against the mesh dimension and displacement count.
5. Preserve multiple-boundary and multiple-`base_name` behavior and the Action's existing NEML2
   property validation.
6. Add Action-level tests proving that normal SCZM Physics syntax creates the Kokkos object graph;
   focused tests that instantiate test objects directly do not cover this requirement.

Success criterion: a documented `[Physics/SolidMechanics/ShiftedCohesiveZone]` input creates and runs
the complete Kokkos SCZM pipeline without manually naming test-only objects.

### 9.5 Required: Host Small-Strain Formulation Parity

The first Kokkos kernel intentionally implements only the traction-jump contribution. Production
parity still requires:

1. Port the optional directional residual correction based on primary/neighbor bulk stress and the
   true-normal/surrogate-normal tangential direction.
2. Port its consistent Jacobian for both supported tangent definitions:
   `stress_wrt_strain` and `pk1_wrt_deformation_gradient`.
3. Port the volumetric-locking correction consistently with the bulk formulation.
4. Support and test `directional_correction`, `stress`, `tangent`, `tangent_definition`,
   `volumetric_locking_correction`, and `no_shifted` with behavior matching
   `SCZMInterfaceKernelSmallStrain`.
5. Provide device-resident primary and neighbor stress/tangent properties. Do not add the correction
   until those producers exist; host property reads inside device assembly are not acceptable.
6. Check non-Cartesian coordinate-system behavior. Either implement the host RZ/spherical gradient
   operators or reject unsupported coordinate systems explicitly rather than silently applying XYZ
   formulas.

Success criterion: residual and Jacobian comparisons against the host non-AD SCZM kernel pass with
both directional correction enabled and disabled for every supported coordinate/tangent mode.

### 9.6 Required: Verification, Backends, and Documentation

1. Add direct host-versus-Kokkos residual and assembled-matrix comparisons on identical solution
   vectors, not only finite-difference Jacobian tests.
2. Add final-solution comparisons for representative linear and nonlinear cohesive laws, including
   an irreversible/stateful law.
3. Exercise 1D, 2D, and 3D displacement-component mappings, multiple interfaces, reversed face
   orientation, nontrivial QP permutations, nonuniform meshes, and variables restricted by block.
4. Exercise MPI distributed mesh, threads, restart/recovery, transient old state, and repeated Newton
   evaluations. Use TestHarness mode flags on the same tests where inputs and expected results are
   unchanged.
5. Compile and run on actual CUDA and at least one additional enabled device backend where available.
   Current CPU Kokkos results do not establish CUDA compilation or GPU residency.
6. Profile to confirm no per-Newton geometry or constitutive Host/Device transfer. Optimize atomics or
   introduce face coloring only if profiling demonstrates a bottleneck.
7. The source page for `KokkosSCZMInterfaceKernelSmallStrain` and a syntax-page scope note were
   committed in `1f90377a16`; their documentation build is not yet verified. Add documentation for
   new production materials and any new generic Kokkos interface-material base. Update the SCZM syntax page linked at the start of Section 9 with backend
   selection, supported laws, limitations, and a production example.
8. Keep the focused SCZM tests linked to
   `syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md` and the generic framework interface
   test linked to `syntax/Kokkos/index.md`. Add dedicated source pages for the new production objects
   before completion, then add them to the applicable `design` entries; do not restore a reference to
   the nonexistent `KokkosInterfaceKernel.md`.

### 9.7 Recommended Implementation Order from the Current Snapshot

```text
1. Direct older/rejected-step Exodiff (handoff Task C)
   -> verify: older differs from old; exactly one rejection; failed trial absent from history
2. Clean-HEAD rebuild/rerun and direct ordinary-neighbor dispatch acceptance
   -> verify: committed B repair executes NeighborInit/NeighborCompute on a ghost datum
3. Harden the landed Phase 5c chain
   -> verify: property-by-property host comparison, dimensions/orientations/base names, docs
4. ShiftedCohesiveZoneAction Kokkos object selection
   -> verify: standard Physics input creates the full pipeline
5. Directional correction and consistent bulk tangent
   -> verify: host/Kokkos residual and matrix comparison
6. Stateful nonlinear, MPI, restart, and real GPU coverage
   -> verify: converged fields, state, and no per-Newton host transfer
7. Final user/source documentation and SQA references
   -> verify: documentation build and TestHarness design links
```

Phase 5a is implemented. Phase 5b has an implemented lifecycle with two open acceptance gaps: direct
older/rejected-step coverage and direct observation of ordinary neighbor-material dispatch. Commit
`5d8558cbc5` repairs the latter's ghost datum construction, so the remaining work is a focused
reproducer/coverage assertion rather than reimplementing the repair. The shared oriented-face cache
is in place through `FEProblemBase::getKokkosInterfaceFaceCache()`.

Phase 5c source is implemented by `a52971472d`: the kernel receives traction and tangent from the
production shifted-jump, pure-elastic, and global-traction material chain. Its current test proves a
composed Jacobian is self-consistent on serial CPU; clean-HEAD evidence and direct host property
parity are still required. Phase 5d has not started: the Action selects host objects only, and
host-formulation parity (directional correction, locking correction, bulk tangents) is absent. Phase
6 has not begun. No retained result establishes actual CUDA/XPU execution.

---

## 10. Risk & Mitigation Matrix

| Risk | Impact | Mitigation Strategy |
| :--- | :--- | :--- |
| **QP indexing divergence on inverted faces** | Critical (Severe kinematic error) | Establish explicit physical-point validation in `KokkosInterfaceKernel` tests; verify $\boldsymbol{x}_{\text{elem}}(qp) \approx \boldsymbol{x}_{\text{neigh}}(qp')$ before evaluating fields. |
| **GPU memory access out-of-bounds in CRS matrix** | Critical (CUDA Illegal Address) | Enforce `ElementSideNeighborLayers` RelationshipManager at input construction; verify that `DofMap` sparsity has non-zero entries for all $(elem, neighbor)$ pairs. |
| **Device virtual function invocation attempt** | High (Compile failure / nvlink error) | Use compile-time static dispatch (CRTP or inline templated helpers) for derived kernel hooks; keep all device methods strictly non-virtual. |
| **Atomic contention on shared edge/corner nodes** | Low/Moderate (Potential performance tail) | Profile with NVIDIA Nsight Compute. Since interface faces are codimension 1, contention is typically low. Keep coloring as a secondary optimization pass only if profiling indicates need. |
