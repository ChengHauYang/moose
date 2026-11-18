# Task G — NEML2 → Kokkos MaterialProperty → SCZM directional correction

**Status:** CPU implementation and focused regressions completed; CUDA/XPU execution remains deferred. The required GPU gates are not passed and Task G is not complete until they are exercised.

**Branch:** `neml2-v3-kokkos-solid-mechanics-LD-mac` (the planned `sczm_dev_gpu` branch is unavailable locally; all references and comparisons are anchored to this verified checkout).

**Reference baseline / Starting SHA:** `4e37469f7209ba3f87c6e83024ad5c201e747dbd` (`add TSL using MOOSE/NEML2`). All three prerequisite commits (`f9bc68a4e4`, `87662e202b`, `4e37469f72`) are present, `output_backend = kokkos` exists in `NEML2Action`, the neighbor-property getter `getKokkosNeighborMaterialPropertyByName` exists in `TwoMaterialPropertyInterface.h`, and SCZM directional correction is already implemented using the old isotropic producer `KokkosSCZMComputeLinearElasticBulkStress`.

**Replaces:** `.plans/sczm/task_g_directional_correction.md` (the previous hard-coded linear-elastic `InterfaceMaterial` plan).

**Related:** `.plans/sczm/kokkos_sczm_plan.md` §9.5 and `.plans/sczm/implementation_handoff.md` §11; update links if these files live elsewhere.

**Target architecture (required):**

```text
[MOOSE interface QPs / displacement gradients, both physical sides]
                        │
                        ▼
   MOOSEQuantityToNEML2 + NEML2BatchIndexGenerator
         (ElemSide(primary) / ElemSide(neighbor))
                        │
                        ▼
           NEML2ModelExecutor / NEML2
           (stress + consistent tangent)
                        │ GPU output tensor
                        ▼
           NEML2ToKokkosMaterialProperty
      (shared staging + index maps + unpacking)
              │                       │
         primary face             neighbor face
       stress / tangent         stress / tangent
              └───────────┬───────────┘
                          ▼
        KokkosSCZMInterfaceKernelSmallStrain
        (standard two-sided property consumer)
                          ▼
        Host-equivalent correction R and J
```

**Not the target:** `KokkosSCZMComputeLinearElasticBulkStress → SCZMInterfaceKernel`; do **not** create a new SCZM-specific constitutive `InterfaceMaterial` or pack the neighbor response into specially named primary properties as the production solution. The existing isotropic producer in the worktree is being migrated to the generic `NEML2ToKokkosMaterialProperty` pipeline.

---

## 1. Outcome and definition of done

Implement small-strain, shifted, non-AD SCZM directional correction with the **same discrete residual and Jacobian as** `SCZMInterfaceKernelSmallStrainTempl<false>`. The stress/tangent must come from **NEML2 output evaluated at both interface faces and published as Kokkos material properties**, with no host read-back in the per-QP execution path.

Task G is complete only when:

1. `directional_correction = true` and `use_kokkos = true` work in 2D and 3D Cartesian shifted SCZM with a user-configured NEML2 model and interface mapping.
2. NEML2 gathers/executes primary **and** neighbor material states at side QPs; each side can have a different bulk constitutive response.
3. The existing `NEML2ToKokkosMaterialProperty` is extended generically to publish side and neighbor-side properties; **no separate interface-only Retriever** is necessary.
4. Primary/neighbor `stress` and a **documented consistent tangent** reside in correct device-addressable face property storage, with working standard two-sided retrieval in the SCZM kernel.
5. No volume-QP offset is reused as a side-QP offset; `interface_only = true` works without element batch indices.
6. The correction matches the host discrete residual, both diagonal Jacobian blocks, and the intentional **zero correction cross blocks**.
7. The heterogeneous interface test demonstrably fails if primary and neighbor stress, tangent, side index, QP ordering, or storage are swapped.
8. Correction-off and `no_shifted = true` cases continue to run without requiring NEML2, new properties, or new materials.
9. CPU baseline, finite-difference Jacobian checks, MPI/distributed ghost-interface tests, and at least one real GPU build **and execution** pass; record CUDA and XPU separately.
10. Unsupported physics/backend/derivative combinations fail early and explicitly rather than silently supplying an incorrect tangent.

**Do not claim full host SCZM parity** until volumetric locking correction, coordinate-system extensions (RZ/RSPHERICAL), and alternate tangent semantics are addressed in §10 (`no_shifted` is already implemented and verified, and must be preserved).

---

## 2. Scope, prerequisites, and non-goals

### 2.1 Supported initial scope (G1)

- Small strain, non-AD, shifted SCZM; Cartesian 2D and 3D.
- Internal two-sided interfaces only. Generic external-boundary NEML2-to-Kokkos output requires a one-sided boundary cache/dispatcher and is outside Task G.
- Existing `KokkosSCZMInterfaceKernelSmallStrain` remains the cohesive traction consumer and receives a directional correction.
- NEML2 stress evaluated at side QPs, with independent primary/neighbor constitutive states.
- `tangent_definition = stress_wrt_strain`: **algorithmic** `dσ/dε` of the same NEML2 stress output, at the same state and same interface QPs.
- Both stress and tangent are supported as configurable **standard output property names**; defaults should follow existing host `stress`, `Jacobian_mult` conventions where feasible.
- A simple linear-elastic **NEML2** model is the first integration fixture, **not** a hard-coded C++ constitutive model. A nonlinear NEML2 model can be used after validating its consistent-tangent and state contracts; the Retriever must not assume isotropic elasticity.
- Explicit restriction: displacement-dependent stress with other constitutive inputs (such as temperature or internal state variables) prescribed or frozen. Active coupled non-displacement dependencies (which would contribute off-diagonal Jacobian blocks) are out of scope for G1 unless corresponding derivatives are provided.
- Existing pure-cohesive / correction-off Kokkos inputs and `no_shifted = true` inputs remain valid without NEML2.

### 2.2 Baseline contracts and prerequisites verified in worktree

Audit of the clean checkout at `4e37469f72` confirms:
- Prerequisite commits `f9bc68a4e4`, `87662e202b`, and `4e37469f72` are all present in this worktree.
- `output_backend = kokkos` is supported in `NEML2Action`.
- The neighbor-property getter `getKokkosNeighborMaterialPropertyByName` is available in `TwoMaterialPropertyInterface.h`.
- Directional correction is already implemented in `KokkosSCZMInterfaceKernelSmallStrain`, currently consuming the isotropic producer `KokkosSCZMComputeLinearElasticBulkStress`.
- `no_shifted = true` is already implemented and verified in Kokkos (`sczm_angled_noshifted_kokkos_matches_host` and `sczm_angled_noshifted_jacobian`).

Required contracts to preserve and complete during migration:
- Generic side batch mapping in `NEML2ToKokkosMaterialProperty` using `ElemSide(P, sP)` and `NeighborSide(N, sN)`.
- Standard two-sided retrieval in `KokkosSCZMInterfaceKernelSmallStrain` using `getKokkosMaterialPropertyByName` and `getKokkosNeighborMaterialPropertyByName`.
- Migration of kernel handles, Action validation, input files, and negative tests from `KokkosSCZMComputeLinearElasticBulkStress` to NEML2.

### 2.3 Explicitly unsupported until verified or implemented

- `volumetric_locking_correction = true`.
- RZ and RSPHERICAL small-strain constitutive kinematics.
- `tangent_definition = pk1_wrt_deformation_gradient`; never reinterpret `dP/dF` as `dσ/dε`.
- A stress/tangent pair using mismatched frames, strain measures, units, derivative targets, or state points.
- A NEML2 constitutive model with **cross-side constitutive dependence**: the host zero correction cross blocks assume each bulk stress responds only to its own side's displacement kinematics. A genuinely coupled interface model needs a different Jacobian contract.
- Unverified device stateful/history synchronization or unsupported output tensor types/layouts.
- A silent CPU fallback or Host material-property access in the device interface kernel.
- Note on `no_shifted`: `no_shifted = true` is already implemented and tested in Kokkos (`sczm_angled_noshifted_kokkos_matches_host` and `sczm_angled_noshifted_jacobian`). It suppresses area and directional corrections without requiring bulk properties, and must be preserved during migration.

### 2.5 G0 addressing audit: results

The G0 spike was run against this checkout before any SCZM code was touched. It confirmed that most of the framework contract the plan was worried about already exists, and it corrected two assumptions in §3.2 and §5.3.

**Two-sided storage is real, not aspirational.** `FEProblemBase` owns three independent Kokkos property storages and allocates all three (`KokkosFEProblemBase.K:468-470`): `_kokkos_material_props` (element), `_kokkos_bnd_material_props` (boundary/face/interface), and `_kokkos_neighbor_material_props` (neighbor). `getKokkosMaterialPropertyByName` resolves against the first, `getKokkosNeighborMaterialPropertyByName` against the third (`TwoMaterialPropertyInterface.h:110-113` forwarding to `_neighbor_material_data`). A single property name therefore yields two genuinely distinct arrays, each shaped `assembly.getNumFaceQps(sid)`. The `J_en`/`J_ne` correction blocks stay zero without any special naming.

**Which storage an object declares into is a per-class choice.** `MaterialPropertyInterface::getMaterialDataType()` honors a `_material_data_type` input parameter before falling back to boundary/block inference (`MaterialPropertyInterface.C:274-282`), and `KokkosFEProblemBase.K:327-347` maps `NEIGHBOR_MATERIAL_DATA` to `_kokkos_neighbor_material_props` while `INTERFACE_MATERIAL_DATA` maps to `_kokkos_bnd_material_props`. A class that sets `_material_data_type` in its own `validParams()` chooses its storage, exactly as the host `InterfaceMaterial.C:20` does. This is what lets one shared Retriever core feed two thin concrete classes.

**Side and neighbor face dispatch already exist.** `Moose::Kokkos::InterfaceMaterial` (`framework/include/kokkos/materials/KokkosInterfaceMaterial.h`) dispatches `InterfaceCompute` once per oriented interface face and hands the derived class `elem_datum`, `neighbor_datum`, `qp`, and `neighbor_qp`. Its documented contract states that `neighbor_qp` is the neighbor-side index at the same physical location. `makeNeighborInterfaceDatum()` builds the neighbor `Datum` from `(face.neighbor, face.neighbor_side)` (`KokkosInterfaceDatum.h:62-70`), so `neighbor_datum.propertyIdx()` is the neighbor element's own face-QP offset, not the primary face's. `InterfaceFaceCache` constructs `_neighbor_qp_map` by matching the two sides' physical QP coordinates and rejects unequal QP counts, missing matches, ambiguous matches, and duplicate neighbor-QP use (`KokkosInterfaceFaceCache.K:82-131`). The map is shared with interface materials and kernels on the same boundaries. Plan §5.3's "build a validated QP permutation" work is therefore **already done by the framework** and must be reused, not reimplemented.

**The NEML2 side-batch half is complete.** `NEML2BatchIndexGenerator` already indexes `ElemSide` for both sides, including the neighbor's own `(neighbor_elem, neighbor_side)` pair (`NEML2BatchIndexGenerator.C:106,116`), and `MOOSEQuantityToNEML2` already gathers `DataSource::ElemSide`, `NeighborSide`, and `Interface` (`MOOSEQuantityToNEML2.C:219-233`) with `interface_only` support. The **host** `NEML2ToMOOSEMaterialProperty` already consumes `getSideBatchIndex` when `_bnd` (`NEML2ToMOOSEMaterialProperty.C:87-93`). Only the Kokkos Retriever lacks side support.

**Consequences for the plan:**

- Phase 1 (G0-A) shrinks from "prove the addressing contract" to "regression-test a contract the framework already provides", and the fixture is the T04 sentinel test. It is still required, but it is no longer an architecture-feasibility spike.
- The Phase 1 fixture must contain at least one non-identity correspondence (`neighbor_qp != qp`) and use a coordinate-based analytic property oracle. This verifies that producers and consumers use the supplied indices correctly without implementing a second QP-matching algorithm.
- Phase 2 and Phase 3 become one implementation: side and neighbor retrievers are the same code with a different base, storage, and side of the datum.
- §3.2's `kokkosElementSideID(tid)` is superseded. The face work-item index comes from `kokkosInterfaceFace(tid)` inside `InterfaceCompute`.
- If one-sided boundary dispatch is added later, share only the topology representation rather than forcing an invalid neighbor into `InterfaceFaceRecord`:

  ```cpp
  struct ElementSideRecord
  {
    ContiguousElementID elem;
    unsigned int side;
  };

  struct BoundaryFaceRecord
  {
    ElementSideRecord side;
    unsigned int n_qps;
  };

  struct InterfaceFaceRecord
  {
    ElementSideRecord primary;
    ElementSideRecord neighbor;
    dof_id_type qp_map_offset;
    unsigned int n_qps;
  };
  ```

  This representation refactor is not required for Task G and does not by itself make `InterfaceMaterial` support external boundaries: the current cache and dispatcher require a real neighbor.
- No framework gap was found for Task G's internal two-sided interface, so §7.2's "implement the smallest generic change" clause is **not triggered**: no new public API, and no `Kokkos::InterfaceMaterial` subclass is needed. PR A in §14 is withdrawn unless later phases expose a gap.

### 2.4 Non-goals

- Reimplementing linear elasticity, return mapping, or crystal plasticity inside the SCZM kernel.
- Creating `KokkosSCZMComputeLinearElasticBulkStress` as a required producer.
- Adding an `NEML2ToKokkosInterfaceMaterialProperty` duplicate Retriever.
- Broad refactoring of unrelated host SCZM code, NEML2 action syntax, or Kokkos storage.
- Claiming GPU zero-copy: an existing `Kokkos::deep_copy` can be device-to-device, but it is still a copy.

---

## 3. Non-negotiable architecture and ownership

### 3.1 Separate producer from consumer

- **NEML2** computes bulk stress and the requested algorithmic derivative at *interface* QPs.
- **`NEML2ToKokkosMaterialProperty`** stages NEML2 output and unpacks into standard Kokkos MaterialProperty storage. It must not know SCZM, traction, normals, Lame parameters, or cohesive laws.
- **`KokkosSCZMInterfaceKernelSmallStrain`** reads primary/neighbor stress/tangent only and applies host-equivalent geometry and FE contractions. It must not execute NEML2 or compute constitutive updates.
- **`ShiftedCohesiveZoneAction`** forwards configuration and checks feasibility. It must not synthesize a SCZM-specific bulk material.

### 3.2 Three separate index/storage concepts

Never conflate:

1. **NEML2 batch offset:** the start of an `ElemSide` output segment in one NEML2 executor's tensor.
2. **Kokkos face work-item index:** one oriented `(elem, side)` in `kokkosInterfaceFace(tid)` under `InterfaceCompute`; may include a neighbor.
3. **Kokkos MaterialProperty storage index:** `property(datum, qp)` based on the designated block/face/neighbor storage and its own QP offset.

A *work-item-to-NEML2-offset* mapping is necessary; a `Datum::propertyIdx()` or `getElemFacePropertyIndex()` must **not** be used as a NEML2 batch offset.

### 3.3 Primary and neighbor are physically different sides

For oriented face `f = (P, sP)`, with neighbor `(N, sN)`:

```text
primary_batch_start[f]  = getSideBatchIndex(ElemSide(P.id, sP))
neighbor_batch_start[f] = getSideBatchIndex(ElemSide(N.id, sN))
```

**Illustrative APIs only; use actual types in the worktree.** `NEML2BatchIndexGenerator` may hold the two sides in one executor or distinct executors; the implementation must resolve this explicitly. A mapping must associate each side with the **correct executor/output tensor**, not assume both live in one contiguous tensor.

### 3.4 Standard two-sided storage, not primary packing

Preferred consumer contract:

```cpp
// Illustrative interface; confirm actual API and signatures.
_stress          = getKokkosMaterialPropertyByName<Real, 2>(stress_name);
_stress_neighbor = getKokkosNeighborMaterialPropertyByName<Real, 2>(stress_name);
_tangent         = getKokkosMaterialPropertyByName<Real, 4>(tangent_name);
_tangent_neighbor= getKokkosNeighborMaterialPropertyByName<Real, 4>(tangent_name);

sigma_p = _stress(elem_datum, qp);
sigma_n = _stress_neighbor(neighbor_datum, neighbor_qp);
```

The tangent is rank-four (`MaterialProperty<Real, 4>`, §6.4 and §9.1), not a compact rank-two `{6, 6}` block. Every residual and Jacobian contraction loop must be bounded by the mesh dimension `dim = kokkosAssembly().getDimension()` rather than a hard-coded component count. The current isotropic implementation loops `make_range(3)` over a `Moose::Kokkos::Real3` tangent, which indexes past the `{dim, dim, dim, dim}` storage in 2D; the loops carry the same requirement into the NEML2 path.

The current public Kokkos `Material::operator()(NeighborCompute, tid, ...)` constructs its `Datum` from the **primary oriented side**, not from the neighbor side. `Moose::Kokkos::Material::computeQpProperties(qp, datum)` therefore sees only `datum.elemID()`, which is the primary element of the oriented face, so a plain `Material` cannot recover the face work-item index on device. **This was G0's key framework spike; its result is recorded in §2.5 and drives the design in §6.**

- Do **not** work around the gap by writing special `*_neighbor` properties to primary `elem_datum` in production.
- Use `Moose::Kokkos::InterfaceMaterial` for the side and neighbor retrievers: it runs once per oriented interface face and already supplies `elem_datum`, `neighbor_datum`, `qp`, and `neighbor_qp`, reusing the shared face cache, the primary/neighbor orientation rules, and the framework-resolved neighbor QP permutation.

### 3.5 Data lifetime and GPU safety

- Host-side map construction occurs after batch indices have been generated for that execution; refresh whenever mesh, batch layout, execution grouping, or interface records change.
- Stage output only when `outputReady()` and its tensor is valid. Preserve/treat step-zero and state initialization rules explicitly.
- Deep copy device→device when source tensor and Kokkos allocation use compatible GPU memory spaces; verify actual backend, stream/execution-space synchronization, and pointer lifetime.
- No direct CPU tensor access from a Kokkos functor. No runtime `std::map`/`unordered_map` lookup, allocations, exceptions, streams, or string manipulation in device code.
- Bind outputs to the correct nonlinear/timestep execution and ensure the assembly reads **current** stress/tangent, not previous-iteration values.

---

## 4. Host oracle: residual and Jacobian to reproduce exactly

Reference the *actual* branch source, starting from:

- `modules/shifted_boundary_method/src/interfacekernels/SCZMInterfaceKernelSmallStrain.C` (baseline residual around lines 146–176; Jacobian around 248–319, diagonal/off-diagonal around 330–380).
- `modules/solid_mechanics/src/utils/ElasticityTensorTools.C` for index/contraction semantics.

Let `i` be row displacement component and `k` trial displacement component. For each oriented face QP:

```text
true_normal       = trueNormal(elem_datum, qp)
surrogate_normal  = elem_datum.normals(qp)
normal_tangent    = surrogate_normal
                  - dot(true_normal, surrogate_normal) * true_normal
```

### 4.1 Residual directional correction

```text
Element:  - test_primary  * Σ_j stress_primary(i,j)  * normal_tangent(j)
Neighbor: + test_neighbor * Σ_j stress_neighbor(i,j) * normal_tangent(j)
```

- The base cohesive residual retains its existing shifted/area-corrected treatment unchanged.
- **Do not multiply the directional correction by `areaCorrection()`.**
- With correction off, no stress/tangent properties may be required.
- Use the *true physical side's* stress output from NEML2; never use element-interior QP stress as an unverified surrogate for face stress.

### 4.2 Jacobian directional correction (`dσ/dε`)

Let `gp` be the **natural, unshifted** gradient of the trial shape function, and `t` the natural unshifted test function on the row side. `C` is the same-side algorithmic tangent with defined indices `C(i,j,k,l) = ∂σ(i,j)/∂ε(k,l)` in the host contraction convention.

```text
origin_part = t * Σ_j Σ_l normal_tangent(j) * C(i,j,k,l) * gp(l)

symmetric_part = t * Σ_m Σ_l normal_tangent(m) * C(i,m,l,k) * gp(l)

correction_jacobian = 0.5 * (origin_part + symmetric_part)
```

The **tensor index order matches the host implementation** (`SCZMInterfaceKernelSmallStrain.C:283-287`):
- `i`: residual displacement component (row)
- `k`: trial displacement component (column)
- `j` or `m`: normal tangent vector component
- `l`: trial gradient vector component
- `gp`: trial shape function gradient `grad_phi`
- `t`: natural test function

In `origin_part`, `C(i, j, k, l)` contracts `normal_tangent(j)` on the second index, `k` on the third, and `gp(l)` on the fourth.
In `symmetric_part`, `C(i, m, l, k)` contracts `normal_tangent(m)` on the second index, `gp(l)` on the third, and `k` on the fourth.

> [!IMPORTANT]
> The transposition `C(i, l, m, k)` is mathematically incorrect even for isotropic elasticity: for `i=0, k=1`, `normal_tangent` along $x$ ($m=0$) and trial gradient along $y$ ($l=1$), the host contraction `C(0, 0, 1, 1)` yields $\lambda$, whereas `C(0, 1, 0, 1)` yields $\mu$, causing the average to evaluate to $(\lambda+\mu)/2$ instead of $\lambda$. The implementation oracle is `C(i, m, l, k)`.

| DG block | Correction contribution | Tangent side | FE data |
|---|---:|---|---|
| `ElementElement` | `-correction_jacobian` | Primary | Primary test + trial gradient |
| `ElementNeighbor` | `0` | None | No correction |
| `NeighborElement` | `0` | None | No correction |
| `NeighborNeighbor` | `+correction_jacobian` | Neighbor | Neighbor test + trial gradient |

- Correction cross blocks are deliberately zero **for the specified independent-side host model**; the existing cohesive traction *base* can still have nonzero cross blocks.
- Add the correction in **both diagonal and displacement off-diagonal assembly** paths.
- Use `neighbor_qp` for neighbor test/gradient and any neighbor property read; do not assume `qp == neighbor_qp`.
- Do not apply `shiftedPhi()` to correction derivatives; it belongs to existing base cohesive traction terms.
- Do not silently replace the 0.5 symmetric contraction with a full-gradient `dP/dF` form.
- For a 2D run, retain a documented 3D embedding convention consistent with Host tensor components.

### 4.3 Stress and tangent must be a compatible pair (bounded constitutive contract)

The output must represent the *same* Cauchy stress and strain measure, frame, current material state, and linearization point. This is essential for nonlinear NEML2: merely having a tensor named `Jacobian_mult` does **not** prove it is the algorithmic `dσ/dε` required by the host Jacobian.

Explicitly check the NEML2 derivative variable selected by `neml2_input_derivative`, including chain rule when the model receives strain **increment**, strain rate, or other transformed inputs. If a consistent derivative is not available, fail G1 validation or defer that model; do not substitute elastic moduli.

**Bounding the nonlinear constitutive contract:**
- G1 is strictly restricted to **displacement-dependent stress** where any other constitutive inputs (such as temperature or internal state variables) are prescribed or frozen.
- If stress depends on another concurrently solved field (such as temperature in coupled thermo-mechanics), returning zero for non-displacement off-diagonal Jacobian blocks is incomplete: the coupled derivative $\partial \boldsymbol{\sigma} / \partial T$ produces an off-diagonal directional correction $\partial R_{\text{dir}, i} / \partial T = - t \sum_j (\partial \sigma_{ij}/\partial T) n_{t, j}$. A consistent $d\boldsymbol{\sigma}/d\boldsymbol{\varepsilon}$ alone does **not** establish a complete coupled Jacobian.
- Any constitutive model with active, concurrently solved non-displacement dependencies must either be explicitly rejected in G1 or provide its corresponding coupled off-diagonal derivatives.

---

## 5. NEML2 gather, execution and side-batch contract

### 5.1 The input/gather path

Expected logical data flow (verify actual code and input syntax):

```text
[NEML2] / interface = ${boundary}
  MOOSEQuantityToNEML2 / executeOnInterface()
    primary:  DataSource::ElemSide(P, sP)
    neighbor: DataSource::NeighborSide(N, sN)
  NEML2BatchIndexGenerator
    side key -> batch start (+ QP count / layout)
  NEML2ModelExecutor
    outputs: stress and d(stress)/d(strain)
```

Check whether `interface_only = true` excludes element indices; if so, side-only production must not attempt any volume-index access at construction, batch setup, or compute time.

### 5.2 Check the physical side correspondence

The host-side index builder must produce a record for each participating oriented interface work item:

```cpp
// Pseudocode; not a guaranteed project API.
struct SideSource {
  ExecutionGroupOrExecutorID source;
  dof_id_type batch_start;
  unsigned int n_qps;
};

primary_source[face_tid]  = locate(P, sP);
neighbor_source[face_tid] = locate(N, sN);
```

- Build using true libMesh IDs and side numbers; contiguous Kokkos IDs are **not** interchangeable with libMesh `elem->id()`.
- Match Kokkos `face_tid` to the identical oriented face; do not depend on unstable iteration orders.
- Missing or duplicate `ElemSide` keys must be reported before GPU launch with face, side, boundary and executor context.
- Set an explicit invalid sentinel for every unused map entry; never leave a device index uninitialized.
- Verify NEML2 batch dimensions: scalar/broadcast output, per-QP output, flattened batch strides, and full tensor rank.

### 5.3 QP ordering and neighbor orientation

NEML2's side-batch indexing and the interface kernel's local QP indices must refer to the same physical locations. The framework contract is authoritative:

- `InterfaceFaceCache` matches primary and neighbor physical QP coordinates on Host, stores the resulting one-to-one correspondence in `_neighbor_qp_map`, and rejects unequal QP counts, missing matches, ambiguous matches, and duplicate neighbor-QP use.
- `Moose::Kokkos::InterfaceMaterial` passes `qp` and the corresponding `neighbor_qp` to the Retriever. The derived Retriever must use those indices directly; it must not compare coordinates or build another permutation.
- Verify that the NEML2 primary side batch is addressed by `qp` and the neighbor side batch by `neighbor_qp`, without assuming they are equal.
- Phase 1 must exercise at least one non-identity correspondence and check a coordinate-based analytic property oracle, so a producer or consumer that uses the wrong supplied index fails.
- Unsupported quadrature or topology is rejected while building the framework cache; no nearest-QP search occurs in device code.

### 5.4 Two block models / two executors

Tests must include primary and neighbor having different material models or parameters. If their NEML2 outputs are produced by distinct executors, the Retriever must associate each side with its proper staged tensor, rather than compute `neighbor_batch_offset` in the primary executor.

Prefer a simple common-executor implementation **only if proven** by the actual `NEML2Action` block/interface semantics. Otherwise add a generic source resolver, or make primary and neighbor Retriever instances each point to their own executor while sharing conversion logic. This is a **stop gate**, not a license to hard-code one-material cases.

---

## 6. Extend `NEML2ToKokkosMaterialProperty` (generic Retriever)

**Modify existing implementation**, not a new SCZM class. Locate the exact `.h` / `.K` files with `git grep -l 'NEML2ToKokkosMaterialProperty'`; the source path/registration in the current worktree is authoritative.

### 6.1 Dispatch model

Confirmed by the G0 audit (§2.5): storage selection follows from `_material_data_type`, and face dispatch comes from `Moose::Kokkos::InterfaceMaterial`.

```text
Element Retriever  -> Moose::Kokkos::Material          -> _kokkos_material_props
Side Retriever     -> Moose::Kokkos::InterfaceMaterial -> _kokkos_bnd_material_props
Neighbor Retriever -> Moose::Kokkos::InterfaceMaterial -> _kokkos_neighbor_material_props
```

Base-class selection cannot depend on a runtime input parameter, so the Retriever is split into **one shared core plus three thin concrete classes**:

```text
NEML2ToKokkosMaterialPropertyCore<rank, full>   staging, batch maps, validation,
                                             tensor conversion, _execute_neml2_model
        |
        +-- NEML2ToKokkosMaterialProperty      element  (existing behavior, unchanged)
        +-- NEML2ToKokkosSideMaterialProperty   interface, writes (elem_datum, qp)
        +-- NEML2ToKokkosNeighborMaterialProperty interface, writes (neighbor_datum, neighbor_qp)
```

The core holds no `MooseObject` state and is not registered; it owns the existing `NEML2ToKokkosMaterialProperty.K` logic unchanged. The three concrete classes are thin: constructor wiring, property declaration, and the device unpack call. This satisfies "one C++ class is the default; a thin shared internal helper is acceptable if the framework requires separate object instances for primary and neighbor material data" (§3.4) — the framework does require them, because one object can declare into exactly one storage.

`NEML2ToKokkosNeighborMaterialProperty` sets `_material_data_type = NEIGHBOR_MATERIAL_DATA` in its `validParams()`, mirroring `InterfaceMaterial.C:20`. `NEML2ToKokkosSideMaterialProperty` inherits `INTERFACE_MATERIAL_DATA` from `Moose::Kokkos::InterfaceMaterial`.

Names must not imply SCZM knowledge: the classes publish a NEML2 output, and nothing about cohesive zones, normals, or Lame parameters may appear in them.

### 6.2 Device map data

```cpp
Moose::Kokkos::Array<dof_id_type> _element_batch_offset;   // element mode only
Moose::Kokkos::Array<dof_id_type> _side_batch_offset;      // per oriented face, side mode
Moose::Kokkos::Array<dof_id_type> _neighbor_batch_offset; // per oriented face, neighbor mode
```

The side and neighbor maps are indexed by the oriented-interface-face work-item index, built on Host from the same `InterfaceFaceRecord` ordering the dispatcher uses, then `copyToDevice()`. Do not call `getSideBatchIndex()` on device.

Indexing rule: a side or neighbor map is **never** populated for a face whose NEML2 batch index does not exist. Every unused entry gets an explicit invalid sentinel (`libMesh::DofObject::invalid_id`) so a missing mapping is a deterministic zero, never an uninitialized device read. Duplicate or missing `ElemSide` keys are reported on Host before launch with the element ID, side, and boundary in the message.

If outputs come from distinct NEML2 executors per side, include a compact source ID per face and make staged-tensor selection explicit rather than assuming one contiguous tensor.

### 6.3 Batch addressing logic

The face work-item index comes from `kokkosInterfaceFace(tid)` inside `InterfaceCompute`, not from `kokkosElementSideID(tid)` and not from a `Datum`. Illustrative side/neighbor unpack:

```cpp
template <typename Derived>
KOKKOS_FUNCTION void
computeQpProperties(const InterfaceFaceRecord & face,
                    Datum & elem_datum,
                    Datum & neighbor_datum,
                    const unsigned int qp,
                    const unsigned int neighbor_qp) const
{
  const auto tid = faceIndex();  // work-item index of this oriented face
  const auto start = isNeighborMode() ? _neighbor_batch_offset[tid] : _side_batch_offset[tid];
  const auto batch = _broadcast_output ? 0 : start + (isNeighborMode() ? neighbor_qp : qp);
  unpackStagedTensor(batch, isNeighborMode() ? _prop(neighbor_datum, neighbor_qp)
                                             : _prop(elem_datum, qp));
}
```

- **Do not apply a QP permutation of your own.** `InterfaceMaterial` already resolved primary-to-neighbor QP correspondence into `_neighbor_qp_map`; `neighbor_qp` is the neighbor-side QP at the same physical location. A hand-built permutation would duplicate the framework and risk disagreeing with it.
- Preserve current element-only behavior exactly; element mode keeps `computeQpProperties(qp, datum)` and `_element_batch_offset[datum.elemID()]`.
- Do not dereference `_element_batch_offset` in side or neighbor mode.
- Keep output tensor identity, batch extent, tensor shape and property shape validated on Host.
- Handle output-not-ready, step-zero initialization, repeated residual/Jacobian evaluation, and lifecycle restart semantics.

### 6.4 Property representation and layout using existing retriever

Retain the standard representation already published by the existing retriever (`framework/include/neml2/materials/NEML2ToKokkosMaterialProperty.h:48-51`):

| Logical property | Kokkos property type | Rationale / Existing Retriever behavior |
|---|---|---|
| Cauchy stress `stress` | `MaterialProperty<Real, 2>` (`NEML2ToKokkosRankTwoMaterialProperty`) | Standard rank-two tensor `σ(i,j)` published directly |
| Algorithmic tangent `Jacobian_mult` | `MaterialProperty<Real, 4>` (`NEML2ToKokkosRankFourMaterialProperty`) | Full rank-four tensor `C(i,j,k,l)` converted from 36-entry Mandel output |

The existing retriever already unpacks 36-entry staged Mandel outputs on device and scales by `mandelFactor(a) * mandelFactor(b)` (`MathUtils::sqrt2` on shear entries) into `prop(i, j, k, l)`.
Do **not** introduce a custom rank-two `{6, 6}` tangent property for G1:
1. `NEML2ToKokkosRankFourMaterialProperty` is already implemented, tested, and shared across MOOSE/NEML2 Kokkos workflows.
2. Property dimensions follow the mesh dimension `dim = kokkosAssembly().getDimension()`. In 2D, stress is `{2, 2}` and tangent is `{2, 2, 2, 2}`; in 3D, stress is `{3, 3}` and tangent is `{3, 3, 3, 3}`. Fixed `{3, 3}` storage across all dimensions would be a separate contract change.
3. Compact Mandel `{6, 6}` storage can follow in future work if profiled memory or throughput measurements justify altering both retriever and kernel contracts.

### 6.5 Staging and synchronization

Keep the present GPU path where valid:

```cpp
// Existing concept, verify source type, device memory space, and lifetime.
Kokkos::View<const Real *, MemSpace, Kokkos::MemoryTraits<Kokkos::Unmanaged>>
    output_view(output.data_ptr<Real>(), output.numel());
Kokkos::deep_copy(_staged_output.deviceView(), output_view);
```

- This avoids host staging **only for compatible device-resident tensors/views**; verify backend/device/rank binding (CUDA/HIP/SYCL) and appropriate copy fencing.
- Ensure NEML2 evaluation completes before staging and staging completes before the material unpack/SCZM assembly; do not rely on accidental stream synchronization.
- If a compatible zero-host-roundtrip path is unavailable on a backend, report unsupported behavior or implement a documented backend-compatible route; do not claim device-only behavior when running a host roundtrip.
- Reuse staging for the set of Retriever outputs where possible but do not introduce unsafe aliasing or lifetime coupling among nonlinear iterations.

---

## 7. Kokkos neighbor-property framework contract (G0 spike)

This is the **highest-risk prerequisite**, not a detail to discover after SCZM is rewritten.

### 7.1 Multidimensional addressing regression (G0-A)

The constant `101/202` fixture distinguishes sides but cannot detect a wrong face offset, work-item stride, QP use, or tensor-component address within one side. G0-A is a regression of the existing framework contract, not a second implementation of QP matching.

Use a multidimensional fixture whose expected value is independently reconstructible from:
- Physical side identity, such as stable element/block identity, not the runtime primary/neighbor label
- A stable physical-face key formed from the canonicalized pair of the two global libMesh `(element ID, side)` identities, rather than the rank-local face work-item index
- Physical QP coordinates
- Tensor component $(i, j)$

For example:

```text
val(side, physical_face, xyz, i, j) = side_term + face_term + 11*x + 101*y + 1009*z
                                       + 10007*i + 100003*j
```

Choose non-overlapping term ranges or compare the terms independently so values cannot collide. Write each side to standard primary and neighbor face property storage, and verify through a minimal Kokkos interface kernel reading `elem_datum, qp` and `neighbor_datum, neighbor_qp`. Do not build a test-only QP permutation: `InterfaceFaceCache` already establishes physical correspondence, and the purpose here is to catch a producer or consumer that misuses the supplied indices or storage.

The test must confirm that:
1. Every physical face, QP location, and rank-two component matches its analytic expected value on both sides, using a non-cancelling mismatch count, maximum error, or sum of squared errors.
2. The fixture contains at least one `neighbor_qp != qp`; otherwise it has not exercised non-identity QP ordering and must not pass this gate.
3. Reversing interface orientation swaps primary/neighbor roles while preserving the physical-side and coordinate oracle.
4. Values remain distinct and correct in serial and across an MPI-partitioned interface with a ghost neighbor; the MPI fixture must demonstrate that at least one tested interface actually crosses rank ownership.

Note: In this checkout, `getKokkosNeighborMaterialPropertyByName` already exists in `TwoMaterialPropertyInterface.h` (and is exercised in `KokkosPenaltyInterfaceDiffusion`). G0-A regresses this contract for tensor-valued face properties under multidimensional addressing rather than introducing a bespoke getter.

### 7.2 If the framework lacks the contract

**Not triggered.** The G0 audit (§2.5) found the contract already present: separate neighbor storage, `_material_data_type`-driven storage selection, and `InterfaceMaterial` face dispatch with a framework-resolved neighbor QP map. No new public API is added and no new `Kokkos::InterfaceMaterial` subclass is needed.

This section is retained only as the contingency if a later phase exposes a genuine gap. In that case implement the smallest generic change needed to support:

1. Retrieving a neighbor Kokkos MaterialProperty by name and registering the proper dependency/storage context.
2. Addressing neighbor material values consistently between producer and consumer, including QP reordering.
3. Correct allocation and dispatch of the neighbor property with boundary/interface restrictions.
4. A standalone framework test covering primary, neighbor, reversed orientation, and distributed mesh.

Keep the change generic and keep the Retriever API unified.

**Architecture feasibility gate:** met by the audit itself. **Stop gate G0-A:** met only after the §11 Phase 1 serial, reversed-orientation, and MPI regression tests pass with a genuinely non-identity QP correspondence.

---

## 8. NEML2 Action and SCZM Action integration

### 8.1 `NEML2Action` — generic outputs

Audit its actual parameter set, type map, output retriever creation, task ordering, and restrictions. Then:

- When Kokkos output is selected, create the Kokkos Retriever matching the output's target side: the element Retriever for a volume output, `NEML2ToKokkosSideMaterialProperty` for a primary-side output, and `NEML2ToKokkosNeighborMaterialProperty` for a neighbor-side output. Use the existing backend selector; do not invent `output_backend` or a new user-facing parameter if an existing one suffices.
- Create the side and neighbor retrievers as **one pair per output**, both restricted to the same interface boundaries, so they share the face cache and the face ordering. A side with no neighbor retriever is a configuration error unless the boundary is external.
- Retain output/derivative names, `base_name`, initialization, and outputReady semantics.
- The two retrievers write to **different storages**, so they are not duplicate writers to one face-storage address. Assert this rather than assume it.
- Preserve the old Host (`NEML2ToMOOSEMaterialProperty`) path and volume Kokkos path.
- Handle `interface_only = true` and NEML2 output arrays without an element-batch map.
- If two sides originate from different NEML2 executors, select proper producer instances and ensure their side restrictions line up with SCZM.

### 8.2 `ShiftedCohesiveZoneAction` migration from isotropic producer to NEML2

In this checkout (`neml2-v3-kokkos-solid-mechanics-LD-mac` at `4e37469f72`), `directional_correction = true` on Kokkos is **already implemented and active**, producing directional corrections via the compact isotropic producer `KokkosSCZMComputeLinearElasticBulkStress`. There is no blanket rejection of `directional_correction` to remove. Instead, §8.2 governs the **explicit migration** from that temporary isotropic producer to the generic NEML2 pipeline:

1. **Remove isotropic producer validation:**
   - Remove `validateKokkosBulkStressMaterial()` (`ShiftedCohesiveZoneAction.C:421-456`), which enforces the user-declared `KokkosSCZMComputeLinearElasticBulkStress`.
   - Remove the parameter rejections on user-set `stress` and `tangent` (`ShiftedCohesiveZoneAction.C:223-232`) that state Kokkos SCZM only reads from `KokkosSCZMComputeLinearElasticBulkStress`.
2. **Integrate NEML2 interface property validation:**
   - Forward user-configured or default `stress` (`stress`) and `tangent` (`Jacobian_mult`), plus `tangent_definition` and `base_name`, to the Kokkos kernel.
   - Extend the NEML2 action validator (`validate_neml2_interface`) to ensure the providing NEML2 block specifies `output_backend = kokkos` and an `interface` covering the SCZM boundary when `use_kokkos = true`.
   - Validate the supported `stress_wrt_strain` derivative contract; return an informative `paramError` if missing or incompatible.
3. **Preserve existing `no_shifted` behavior:**
   - `no_shifted = true` is already implemented and verified in Kokkos (`sczm_angled_noshifted_kokkos_matches_host` and `sczm_angled_noshifted_jacobian`).
   - It suppresses area correction and directional correction completely (`_shifted = !getParam<bool>("no_shifted")`), requiring no bulk stress or tangent properties.
   - **Do not reject `no_shifted = true`**; preserve this mode and its tests unchanged.
   - The preservation claim covers the fixture inputs, not only the code path: `sczm_angled_noshifted_kokkos.i` currently declares `bulk_stress` of type `KokkosSCZMComputeLinearElasticBulkStress`, an unused producer that this migration removes. Drop that block (and the modulus comment above it) from the input while leaving the expected behavior and the gold Exodus file untouched, so T13 actually demonstrates that `no_shifted = true` needs no bulk producer. If removing it turns out to perturb the solution, that is a finding to resolve here, not a reason to keep a dead material.
4. **Preserve explicit guards for unverified physics:**
   - Retain explicit rejection of `volumetric_locking_correction = true` until G2.
   - Retain explicit rejection of `tangent_definition = pk1_wrt_deformation_gradient` until full-gradient tangent support is added.
   - Retain explicit rejection of RZ/RSPHERICAL coordinate systems until out-of-plane kinematics are verified in NEML2.
5. **Inventory of existing artifacts to migrate to NEML2:**
   - **Kernel property handles:** `KokkosSCZMInterfaceKernelSmallStrain.h` replaces `_sczm_bulk_stress`, `_sczm_bulk_stress_neighbor`, `_sczm_bulk_lame`, `_sczm_bulk_lame_neighbor` with standard primary/neighbor `_stress` and rank-four `_tangent`.
   - **Input test files:** Migrate `sczm_angled_kokkos.i`, `sczm_angled_3d_kokkos.i`, `sczm_hetero_kokkos.i`, and `sczm_linear.i` from `type = KokkosSCZMComputeLinearElasticBulkStress` to `[NEML2]` models with interface gatherers and `NEML2ToKokkosMaterialProperty` outputs on the interface.
   - **Negative tests:** Migrate `missing_bulk_material`, `user_set_stress`, `user_set_tangent`, and modulus bounds in `kokkos_shifted_interface/tests` to verify NEML2 interface coverage and backend validation.

### 8.3 Naming and backward compatibility

Default to Host-conventional user-level names (`stress`, `Jacobian_mult`), with existing `base_name` prefix rules. Separate **user-facing logical tensor name** from **Kokkos storage type/layout**. No special `sczm_bulk_stress_neighbor` or `sczm_bulk_lame_neighbor` contract.

When `directional_correction = false` or `no_shifted = true`, constructor and Action must not require or retrieve NEML2 stress/tangent properties.

---

## 9. Kokkos SCZM interface-kernel changes

**Files (baseline paths):**

- `modules/shifted_boundary_method/include/kokkos/interfacekernels/KokkosSCZMInterfaceKernelSmallStrain.h`
- `modules/shifted_boundary_method/src/kokkos/interfacekernels/KokkosSCZMInterfaceKernelSmallStrain.K`

### 9.1 Parameters and property handles

- Add `directional_correction` (Host default `true`, but preserve correction-off backward behavior).
- Add Host-compatible optional `stress`, `tangent`, `tangent_definition`; initial supported derivative is strain-based.
- Keep existing traction/dtraction members and base traction behavior unchanged.
- Replace isotropic Lame property handles with standard primary and neighbor stress and tangent handles:
  - `_stress`: `Moose::Kokkos::MaterialProperty<Real, 2>` via `getKokkosMaterialPropertyByName`
  - `_stress_neighbor`: `Moose::Kokkos::MaterialProperty<Real, 2>` via `getKokkosNeighborMaterialPropertyByName`
  - `_tangent`: `Moose::Kokkos::MaterialProperty<Real, 4>` via `getKokkosMaterialPropertyByName`
  - `_tangent_neighbor`: `Moose::Kokkos::MaterialProperty<Real, 4>` via `getKokkosNeighborMaterialPropertyByName`
- Note that property dimensions follow the mesh dimension `dim`: `{dim, dim}` for stress and `{dim, dim, dim, dim}` for tangent.
- Retrieve bulk properties conditionally only when `_shifted && _directional_correction`.
- Ensure functor/shallow-copy constructors copy every handle/scalar used on device.

### 9.2 Residual

- Compute current base traction residual as before.
- If directional correction enabled and `_shifted = true`, calculate `normal_tangent` and add exactly §4.1.
- Read primary stress at `elem_datum, qp`, neighbor stress at `neighbor_datum, neighbor_qp` through the verified two-sided property contract.
- Correction uses no `areaCorrection()` factor.

### 9.3 Jacobian and off-diagonal Jacobian

One device helper accepts DG block, row/trial displacement components, shape/test indices, `qp`, `neighbor_qp`, and the two datums:

1. Return zero **correction** immediately for `ElementNeighbor` and `NeighborElement`.
2. Select the row-side natural test and same-side unshifted trial gradient.
3. Load the row-side NEML2 rank-four consistent tangent `C(i,j,k,l)`.
4. Evaluate `origin_part` and `symmetric_part` using the exact host contractions from §4.2:
   - `origin_part = t * Σ_j Σ_l normal_tangent(j) * C(i,j,k,l) * gp(l)`
   - `symmetric_part = t * Σ_m Σ_l normal_tangent(m) * C(i,m,l,k) * gp(l)`
5. Apply `-` in `ElementElement`, `+` in `NeighborNeighbor`.
6. Add the correction to existing base-traction derivatives in diagonal and displacement off-diagonal assembly paths.
7. Return only the base cohesive derivative for cross DG blocks.
8. **Bound nonlinear constitutive contract:** Returning zero for non-displacement off-diagonal blocks is strictly restricted to models where stress depends only on displacement (strain), with other inputs (e.g. temperature) prescribed or frozen. If stress depends on a concurrently solved coupled non-displacement variable (such as temperature in coupled thermo-mechanics), the coupled derivative $\partial \boldsymbol{\sigma} / \partial T$ contributes $\partial R_{\text{dir}, i} / \partial T = - t \sum_j (\partial \sigma_{ij}/\partial T) n_{t, j}$. A consistent $d\boldsymbol{\sigma}/d\boldsymbol{\varepsilon}$ alone does not establish a complete coupled Jacobian. Concurrently solved coupled non-displacement fields must be rejected in G1 or provide their corresponding off-diagonal derivatives.

### 9.4 Device-code prohibitions

No device-side dynamic allocation, host `RankTwoTensor`/`RankFourTensor`, exceptions, logging/streams, host property references, unreconciled tensor pointer lifetimes, string operations, or assumptions that primary and neighbor QP indices match.

---

## 10. G2 follow-up: tracked full-parity work

G1 is not a claim of unrestricted constitutive/SCZM parity.

### 10.1 Volumetric locking correction

First prove device-side volume `Datum` / `AssemblyDatum` can be constructed from both interface-adjacent elements (including ghosts), with valid `JxW`, natural shape data and coordinates. Then port host average `gradOpForCoord` and locking correction; do not mix volume-QP property offsets with interface side-QP tensor offsets. Check Cartesian and RZ versus Host. Until demonstrated, reject `volumetric_locking_correction = true`.

### 10.2 `no_shifted` (Preserved baseline behavior)

`no_shifted = true` is already implemented and verified in Kokkos (`sczm_angled_noshifted_kokkos_matches_host` and `sczm_angled_noshifted_jacobian`). When enabled, the base cohesive traction omits the area correction and the directional correction is completely suppressed. The capability, its gold files, and its expected solutions must be preserved through the migration of the directional correction producer to NEML2; the only permitted input change is dropping the unused `bulk_stress` block from the Kokkos fixture, which §8.2 item 3 requires so the tests actually exercise the mode without a bulk producer.

### 10.3 Alternative tangent definitions

Support `pk1_wrt_deformation_gradient` only with a documented NEML2 stress measure, tensor frame, full-gradient derivative `dP/dF`, storage layout (potential 3×3×3×3), and host-compatible origin-only contraction. Never reuse 6×6 `dσ/dε` for arbitrary `dP/dF`.

### 10.4 RZ and RSPHERICAL

Verify the NEML2 gathered strain kinematics include hoop terms and coordinate singularity handling **consistent with Host**. Port required geometry/kinematics, then add nonzero-hoop Host/Kokkos tests. Do not claim support just because the normal projection is dimension-agnostic.

### 10.5 Stateful plasticity / crystal plasticity production certification

Beyond initial G1 linear/nonlinear probes, demonstrate:

- Per-side history isolation and state indexing across timestep, nonlinear iteration, rollback/retry and restart.
- Host/Kokkos equality of stress + **algorithmic tangent**, not only final displacements.
- Consistent strain-increment/rate derivative chain rules and return-mapping state handling.
- MPI ghost-side state ownership/communication, multi-GPU assignment, and lifecycle on mesh changes.
- Real CUDA and (targeted) XPU/HIP runs; memory, synchronization and throughput measurements.

The Retriever should **not** require refactoring to add constitutive models once these NEML2/derivative contracts are satisfied.

---

## 11. Executable implementation sequence and stop gates

Do not proceed past a failed gate by introducing SCZM-specific fallback data storage.

### Phase 0 — baseline audit and migration inventory (G0)

1. **Baseline verified:** branch `neml2-v3-kokkos-solid-mechanics-LD-mac` at SHA `4e37469f7209ba3f87c6e83024ad5c201e747dbd`. (The planned `sczm_dev_gpu` branch is unavailable locally).
2. **Confirmed prerequisites:** commits `f9bc68a4e4`, `87662e202b`, and `4e37469f72` are all present.
3. **Confirmed APIs:** `output_backend = kokkos` exists in `NEML2Action`; `getKokkosNeighborMaterialPropertyByName` exists in `TwoMaterialPropertyInterface.h`; `no_shifted = true` is already implemented and verified in Kokkos SCZM.
4. **Inventory of existing implementation to migrate to NEML2:**
   - **Producer:** `KokkosSCZMComputeLinearElasticBulkStress` (computes isotropic stress and Lame constants on primary face datum).
   - **Consumer property handles in `KokkosSCZMInterfaceKernelSmallStrain`:** `_sczm_bulk_stress`, `_sczm_bulk_stress_neighbor`, `_sczm_bulk_lame`, `_sczm_bulk_lame_neighbor` must migrate to standard `_stress`, `_stress_neighbor`, and rank-four `_tangent`, `_tangent_neighbor`.
   - **Action validation in `ShiftedCohesiveZoneAction`:** `validateKokkosBulkStressMaterial()` and parameter errors rejecting user `stress`/`tangent` must migrate to `validate_neml2_interface` checking boundary coverage and Kokkos backend.
   - **Test inputs:** `sczm_angled_kokkos.i`, `sczm_angled_3d_kokkos.i`, `sczm_hetero_kokkos.i`, and `sczm_linear.i` migrate from `KokkosSCZMComputeLinearElasticBulkStress` to `[NEML2]` blocks.
   - **Negative tests in `kokkos_shifted_interface/tests`:** migrate `missing_bulk_material`, `user_set_stress`, `user_set_tangent` to NEML2 validation tests.
5. Finalize the generic side-batch index contract: mapping `ElemSide(P, sP)` and `NeighborSide(N, sN)` into `NEML2ToKokkosMaterialProperty`.

**Stop gate G0:** Baseline SHA and migration inventory documented; prerequisite commits and framework APIs verified present.

### Phase 1 — multidimensional two-sided addressing regression (G0-A)

The audit in §2.5 answered the design question; this phase converts the answer into a regression test so a later framework change cannot silently break it. This is a test of **existing** framework behavior, not a spike.

1. Produce rank-two sentinel properties through a pair of `Moose::Kokkos::InterfaceMaterial` subclasses into primary and neighbor storage. Define the expected values from stable element/block side identity, a physical-face key formed by canonicalizing both sides' global libMesh `(element ID, side)` pairs, physical QP coordinates, and tensor indices `(i,j)`; do not use runtime primary/neighbor labels or the rank-local face work-item index as the oracle.
2. Consume both through a minimal Kokkos interface kernel using `elem_datum, qp` and `neighbor_datum, neighbor_qp`, and compare every component with the analytic oracle using a non-cancelling error metric.
3. Use multiple interface faces and multiple QPs, and assert that at least one tested face has `neighbor_qp != qp`. Do not construct a second QP map: this test verifies correct use of the framework-provided correspondence.
4. Repeat with reversed interface orientation, verifying physical-side identity rather than merely observing two runtime labels swap.
5. Repeat with MPI ownership crossing the interface, and demonstrate that at least one tested primary/neighbor pair has different owning ranks and a ghost neighbor on the writer rank.

**Stop gate G0-A:** the multidimensional fixture passes in serial, reversed orientation, and MPI; every face/QP/component matches the coordinate-based oracle, at least one correspondence is non-identity, and the MPI fixture actually crosses rank ownership.

### Phase 2 — side and neighbor Retrievers (G1-A and G1-B, merged)

§2.5 showed the side and neighbor paths share everything but the base class, the storage, and which side of the face is written. They are therefore one implementation.

1. Extract the existing `NEML2ToKokkosMaterialProperty` staging, batch indexing, validation, and tensor conversion into `NEML2ToKokkosMaterialPropertyCore<rank, full>`, leaving element behavior byte-for-byte unchanged.
2. Add `NEML2ToKokkosSideMaterialProperty` (`InterfaceMaterial` base, bnd storage) writing at `(elem_datum, qp)`, with a Host-built `_side_batch_offset` indexed by oriented-face work item.
3. Add `NEML2ToKokkosNeighborMaterialProperty` (`InterfaceMaterial` base, `NEIGHBOR_MATERIAL_DATA`, neighbor storage) writing at `(neighbor_datum, neighbor_qp)`, with `_neighbor_batch_offset`. Reuse `InterfaceMaterial`'s `_neighbor_qp_map`; do not hand-build a QP permutation.
4. Set explicit invalid sentinels for faces without a NEML2 batch index, and report missing/duplicate `ElemSide` keys on Host before launch.
5. Wire `NEML2Action` to emit the side/neighbor retriever pair per output, sharing interface restrictions.
6. Test the Retriever pair independently of SCZM on an internal two-sided interface. External-boundary output is outside Task G because the current `InterfaceFaceCache` and `InterfaceMaterial` require a real neighbor.
7. Test `interface_only = true` with no volume batch entries; missing-index behavior must be deterministic.
8. Test distinct primary/neighbor model parameters and, if supported, distinct NEML2 executors.
9. Test a distributed face whose neighbor is a ghost.

**Stop gate G1-A/G1-B:** primary/neighbor stress values are correct, finite, independently configurable and properly oriented through the two distinct storages; element-only regression unchanged; no missing/off-rank batch indices; no Host round-trip on the tested GPU backend.

### Phase 3 — tangent conversion contract (G1-C)

1. Publish Host-consistent Cauchy `stress` and algorithmic `dσ/dε` as Kokkos face/neighbor properties.
2. Verify rank/flattened shape, Mandel factor, symmetry and tensor indices with controlled stress/tangent fixtures (normal + pure shear + mixed modes).
3. Verify same-step/iteration lifetime; verify optional initial-state behavior and stage/fence order.
4. Add tests that deliberately corrupt or swap tensor-side mappings to show tests fail.

**Stop gate G1-C:** side-by-side values of all stress and tangent components match Host and known analytic NEML2 elasticity derivatives; conversion passes on device.

### Phase 4 — SCZM residual correction (G1-D)

1. Add conditional stress retrieval and host-equivalent §4.1 residual to the existing Kokkos kernel.
2. Forward Action parameters with correction on, without changing correction-off behavior.
3. Use an angled interface with provably `normal_tangent != 0` and nonzero stress projection.
4. Compare full residual and a selected QP correction against Host; demonstrate the solution changes from correction off.

**Stop gate G1-D:** Host/Kokkos residual agreement with correction on; correction off and `no_shifted = true` still pass without NEML2 properties.

### Phase 5 — Jacobian and action guards (G1-E)

1. Add the exact symmetric strain-based §4.2 Jacobian (`C(i,m,l,k)` contraction) to both diagonal/off-diagonal paths.
2. Verify correction-only DG block values, including two zero cross blocks.
3. Run `PetscJacobianTester` and a controlled finite-difference check; use a model with a consistent tangent.
4. Validate unsupported modes, missing NEML2 outputs, backend/interface restrictions and incompatible derivative definitions.

**Stop gate G1-E:** Jacobian checks, direct four-block checks, heterogeneous swap tests, and negative tests pass.

### Phase 6 — regression, GPU/MPI, performance, and docs

1. Update four existing angled 2D/3D Host/Kokkos cases to enable correction and **NEML2** output for the Kokkos case.
2. Preserve or regenerate canonical Host gold Exodus files only when needed, and retain relative symlinks for Kokkos equivalents.
3. Run serial, distributed mesh, MPI ghost-interface, correction-off, `no_shifted = true`, interface-only and large/two-sided cases.
4. Build and **execute** on CUDA; compile/run XPU where available (report not-run separately). Verify tensor/device staging and synchronization.
5. Update user docs, code comments, requirements and performance notes; make PRs separable if framework dependency is substantial.

**Merge gate:** all applicable acceptance checks and actual device runs are documented; no claim of unverified GPU speedup or arbitrary-material support.

---

## 12. Required verification matrix

Place SCZM tests under `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/` and generic Retriever/framework tests near their own owners. Preserve `compute_devices = 'cpu cuda xpu'` where supported by the surrounding TestHarness; declarations **do not** count as runs.

Gate on capabilities as well as compute devices. Every test whose input consumes a `[NEML2]` model published through `NEML2ToKokkosMaterialProperty` requires `capabilities = 'kokkos & libtorch & neml2'`, matching the existing NEML2 Kokkos tests; a plain `capabilities = kokkos` would let the test be collected and then fail in a build without libtorch or NEML2. Correction-off and `no_shifted = true` cases are the deliberate exception: they require no NEML2 model, so they keep `capabilities = kokkos` and remain runnable with Kokkos alone. That split is only true once the `no_shifted` fixture stops declaring a bulk producer (§8.2 item 3).

| ID | Test / fixture | Essential assertion |
|---|---|---|
| T01 | Existing element-only NEML2 Kokkos | Existing bulk property output unchanged |
| T02 | Internal interface, side Retriever independent of SCZM | Passed: rank-two stress and rank-four tangent are retrieved from boundary storage, including Mandel shear conversion |
| T03 | `interface_only = true` | Passed: the minimal Retriever fixture uses `interface_only = true` with no volume evaluation |
| T04 | Multidimensional addressing fixture | G0-A passed separately (`7524329254`); non-identity correspondence, reversed orientation, and rank-crossing MPI remain covered there |
| T05 | Two-sided NEML2 model, E=1/3 and ν=.30/.20 | Passed in the SCZM heterogeneous fixture: different stress and tangent values on two sides |
| T06 | Reversed orientation | Primary/neighbor swap only as physics dictates |
| T07 | Nontrivial 2D and 3D QP ordering | At least one `neighbor_qp != qp`; coordinate-based oracle confirms both producer and consumer use `InterfaceMaterial`'s supplied indices, with no test-only permutation |
| T08 | Mandel conversion: pure shear + mixed terms | Correct `√2` factors, `C(i,j,k,l)` contractions |
| T09 | NEML2 output cross-executor case | No accidental use of wrong executor batch offset |
| T10 | Host/Kokkos correction-on, angled 2D/3D | Passed on CPU: residual and Jacobian match the Host oracle |
| T11 | Four correction Jacobian blocks | Passed on CPU: `J_ee`: primary; `J_nn`: neighbor; correction `J_en=J_ne=0` |
| T12 | `PetscJacobianTester` | Passed on CPU: analytic Jacobian matches numerical derivative (`sczm_linear_jacobian` relative error `3.25241e-10`) |
| T13 | Correction off and `no_shifted = true` without NEML2 | Passed on CPU: backward compatibility and no unconditional getter |
| T14 | Distributed mesh `-p2` | Passed on CPU: 16/16 selected distributed-mesh tests |
| T15 | GPU CUDA run | Deferred: not tested |
| T16 | XPU/HIP as targeted/available | Deferred: not tested |
| T17 | Unsupported modes and invalid outputs | Passed on CPU: invalid Kokkos output backend fails during setup without a fallback |
| T18 | Nonlinear NEML2 constitutive probe | Stress/tangent and Newton/Jacobian consistency at one state |

### 12.1 Required negative tests

- Correction enabled with no NEML2-produced side stress/tangent.
- NEML2 material output only on element QPs (missing `interface` declaration).
- `interface` restriction does not overlap the SCZM boundary.
- Unsupported output backend; incompatible tensor output type/shape or missing derivative.
- No side-batch index for primary or neighbor, invalid `interface_only` mapping, duplicate source map.
- Mismatched source executor, unsupported neighbor ghost-side batch.
- `volumetric_locking_correction = true` before G2.
- `pk1_wrt_deformation_gradient` with G1 strain-based storage.
- RZ/RSPHERICAL before verified G2 kinematics.
- Invalid or mismatched displacement count, unsupported NEML2 strain input semantics.
- Stateful or cross-side-coupled model without a validated derivative/state contract.

### 12.2 Host/Kokkos numerical tolerances

The existing Kokkos-vs-Host Exodiff comparisons in this suite run at `abs_zero = 1e-8`, `rel_err = 1e-8`, with `PetscJacobianTester` at `ratio_tol = difference_tol = 1e-7`. Keep those values for the comparable CPU path; do not loosen them to hide discrepancies. Tightening them to `1e-10` is a proposed change, not a description of the current state: it requires the migrated tests to pass at that tolerance against unchanged gold files on the tested build before the plan can claim it, and any tolerance change must state the evidence that motivated it. For GPU, record precision, compiler, reduction order, absolute/relative error and a justified separate tolerance only where real device floating-point ordering requires it. Compare individual stress/tangent and residual/Jacobian components, not just final Exodus fields.

### 12.3 Multi-backend and MPI

- Run the 2D angled case in distributed mode (TestHarness equivalent of `-p2 --distributed-mesh`) with suitable thread/job slots.
- Test an actual interface crossing rank ownership, not merely any two-rank mesh.
- Verify mapping invalidation/rebuild after mesh changes and per-execution regrouping.
- Exercise one-rank-per-GPU device selection and memory ownership where possible; confirm CUDA tensor device matches Kokkos execution device.
- Detect use-after-free, stale staged output and missing fences under repeated nonlinear iterations.

---

## 13. Verification procedure and evidence

Before running a build or an existing executable, confirm whether the MOOSE stack requires conda and the correct named environment; do not trust an inherited shell or stale binary. Record build flags for `MOOSE_KOKKOS_ENABLED`, `NEML2_ENABLED` and the active execution backend.

After environment/build setup, from `modules/shifted_boundary_method`:

```bash
./run_tests -j2 --re 'kokkos_shifted_interface'
```

Also run dedicated framework Kokkos neighbor-property and NEML2 Retriever test suites, plus focused negative tests and the affected module regressions. Confirm test selection names from the actual repository; do not assume the regex above covers new generic Retriever tests.

Every verification record must include:

1. SHA, dirty/clean working tree and relevant prerequisite commit status.
2. Exact compilation command, environment and whether the executable was freshly rebuilt.
3. Test commands, pass/fail/skip totals, Host-vs-Kokkos tolerances and gold source.
4. Serial vs MPI/distributed mesh results and whether the interface truly spans ranks.
5. Backend actually used (`cpu`, `cuda`, `xpu`, `hip`) and device IDs.
6. NEML2 execution mode, whether stress/tangent were newly evaluated and whether the tensor was on Host or GPU.
7. Stage copies, synchronization barriers, H2D/D2H transfers and peak GPU memory/performance when measured.

A declared compute device or a CPU-only build does **not** count as GPU validation. An untested platform must be marked **not tested**, not passed.

### 13.1 Recorded CPU evidence

On the `moose` conda environment, the focused generic Retriever command
`./run_tests -j3 --re 'neml2\\.kokkos_interface_retriever'` passed 3/3. The fixture uses
`interface_only = true`, checks primary and neighbor rank-two stress and rank-four tangent
(including Mandel shear conversion), exercises `interface_output_side = both|primary|neighbor`,
and verifies deterministic zero writes for an interface with no NEML2 batch entry.

The CPU SCZM suite passed 56/56 at one MPI rank and the selected two-rank distributed-mesh
coverage passed 16/16. `sczm_linear_jacobian` reported a finite-difference relative error of
`3.25241e-10` under the existing `1e-7` tolerance; no tolerance was relaxed. CUDA and XPU/HIP
execution were not run. The available `moose-dev` package is `2026.08.23`, below the warned
`2026.09.10` version, so no device-environment upgrade was attempted.

---

## 14. Documentation and PR organization

Update:

- `doc/content/syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md`
- `doc/content/source/interfacekernels/KokkosSCZMInterfaceKernelSmallStrain.md`
- NEML2/Kokkos material-property documentation and side/interface output examples (correct paths discovered from the worktree)
- Relevant framework Kokkos neighbor-property docs **if** new public APIs are added.
- `.plans/sczm/kokkos_sczm_plan.md` and `.plans/sczm/implementation_handoff.md` so older G1 linear-elastic-specific guidance cannot accidentally be reused.

Document:

1. End-to-end architecture, source/consumer ownership and naming conventions.
2. NEML2 `interface` input and real Kokkos backend-selection syntax.
3. How to output `stress` and the **correct algorithmic tangent**, including Mandel layout and derivative target.
4. Two-sided interface setup, model restrictions and material-block heterogeneity; state explicitly that generic external-boundary output is outside Task G.
5. Device execution requirements and limitations; correction-off backward compatibility.
6. G1 restrictions and G2 parity items.

Suggested PR split when code review benefits:

- **PR A — Generic Kokkos neighbor property framework:** withdrawn. The G0 audit found no gap, so no framework change is expected. Restore this PR only if a later phase proves one is needed.
- **PR B — NEML2ToKokkosMaterialProperty Side/Neighbor support:** generic side mappings, storage, tensor conversion, and tests.
- **PR C — SCZM consumer + Action + Host parity:** kernel contractions, validation, focused SCZM tests.

Do not combine a large generic neighbor-property API overhaul and SCZM-specific code without distinct commits and independent tests.

---

## 15. Implementing-agent review checklist

**Prerequisites, Framework, and Addressing Proof**

- [ ] Branch `neml2-v3-kokkos-solid-mechanics-LD-mac` and baseline commit `4e37469f72` verified.
- [ ] Confirmed prerequisites present: commits `f9bc68a4e4`, `87662e202b`, `4e37469f72`, `output_backend = kokkos`, and `getKokkosNeighborMaterialPropertyByName`.
- [ ] G0 audit recorded in §2.5: separate neighbor storage, `_material_data_type`-driven storage selection, `InterfaceMaterial` face dispatch with a resolved neighbor QP map, and complete NEML2 `ElemSide` indexing. No framework gap, so §7.2 is not triggered.
- [ ] Stop gate G0-A passed: Multidimensional addressing fixture checks physical side, stable face identity, physical-coordinate oracle, and every tensor component across multiple faces/QPs using a non-cancelling error metric; at least one `neighbor_qp != qp`; reversed orientation and a genuinely rank-crossing MPI interface pass.

**Retriever (`NEML2ToKokkosMaterialProperty`)**

- [ ] Existing `NEML2ToKokkosMaterialProperty` refactored into `NEML2ToKokkosMaterialPropertyCore` plus thin element/side/neighbor classes; element behavior unchanged and no SCZM knowledge in any of them.
- [ ] Side and neighbor retrievers derive from `Moose::Kokkos::InterfaceMaterial` and reuse its face cache, orientation rules, and `_neighbor_qp_map`; no coordinate rematching or hand-built QP permutation. Scope remains internal two-sided interfaces because the dispatcher requires a real neighbor.
- [ ] Retains rank-four tangent (`NEML2ToKokkosRankFourMaterialProperty`) and rank-two stress representation; property dimensions follow mesh dimension `dim`, and every contraction loop is bounded by `dim` rather than a fixed component count.
- [ ] Element/Side/Neighbor mode dispatch is correct; no volume batch offset used for side QPs; `interface_only = true` works without element indices.
- [ ] Both `ElemSide(P,sP)` and `ElemSide(N,sN)` map to correct physical side batch and QP ordering without assuming `qp == neighbor_qp`; unused face entries carry an explicit invalid sentinel.
- [ ] Device-to-device stage copies verified without host read-back; existing element-only Kokkos Retriever tests pass.

**SCZM Kernel and Action Migration**

- [ ] Migrated completely from `KokkosSCZMComputeLinearElasticBulkStress`: property handles replaced with standard `_stress`, `_stress_neighbor`, and rank-four `_tangent`, `_tangent_neighbor`.
- [ ] Directional residual excludes `areaCorrection()`.
- [ ] Directional Jacobian uses exact host contractions: `origin_part` with `C(i,j,k,l)` and `symmetric_part` with `C(i,m,l,k)`.
- [ ] Diagonal blocks use independent same-side tangents; directional cross-block corrections are zero.
- [ ] Nonlinear constitutive contract bounded: displacement-dependent stress with other inputs prescribed/frozen, or coupled non-displacement off-diagonal derivatives provided.
- [ ] Existing `no_shifted = true` behavior and gold preserved; its Kokkos fixture declares no bulk producer, so `capabilities = kokkos` alone still runs it. Correction-off requires no NEML2 properties.
- [ ] Action validation migrated: `validateKokkosBulkStressMaterial()` removed; NEML2 interface coverage and `output_backend = kokkos` validated.
- [ ] Unverified features (`volumetric_locking_correction`, `pk1_wrt_deformation_gradient`, RZ/RSPHERICAL) remain explicitly rejected.

**Verification, SQA Standards, and Handoff**

- [ ] C++ code formatted with the mandatory command: `pre-commit run clang-format --files <modified-files>`, passing with no diff.
- [x] Every new or modified test in TestHarness `tests` specifications includes explicit `requirement`, `design`, and `issues` metadata.
- [x] Capability gating is correct: NEML2-consuming tests declare `capabilities = 'kokkos & libtorch & neml2'`, while correction-off and `no_shifted = true` tests stay at `capabilities = kokkos`.
- [x] Host oracle comparison on angled 2D and 3D interfaces matches within the existing tight tolerances on CPU.
- [x] Heterogeneous interface, reversed-orientation, and `PetscJacobianTester` pass on CPU.
- [x] Distributed mesh (`-p2`) selected tests pass on CPU.
- [x] Negative tests fail early and informatively for invalid Kokkos output backend setup.
- [ ] Actual CUDA compilation and execution logged (deferred; CUDA and XPU/HIP not tested).

**Do not mark Task G complete while any required item is unknown or failed.**

---

## 16. Concise handoff instruction for an implementing agent

> Implement Task G using the **NEML2 → `NEML2ToKokkosMaterialProperty` → `KokkosSCZMInterfaceKernelSmallStrain`** path on baseline branch `neml2-v3-kokkos-solid-mechanics-LD-mac` (HEAD `4e37469f72`). The G0 audit in §2.5 is already done; read it before writing code.
>
> 1. **No bespoke storage:** Split the Retriever into `NEML2ToKokkosMaterialPropertyCore` plus thin element/side/neighbor classes. Side and neighbor derive from `Moose::Kokkos::InterfaceMaterial`; the neighbor class sets `_material_data_type = NEIGHBOR_MATERIAL_DATA` so the two sides land in two distinct storages. Write the side at `(elem_datum, qp)` and the neighbor at `(neighbor_datum, neighbor_qp)`. Retain rank-four tangent storage (`MaterialProperty<Real, 4>`) with dimensions matching mesh dimension, and bound every contraction loop by `dim`.
> 2. **Reuse, do not rebuild:** the face cache, orientation rules, and neighbor QP permutation already exist in `InterfaceMaterial`; the NEML2 `ElemSide` batch indices already exist in `NEML2BatchIndexGenerator`/`MOOSEQuantityToNEML2` for both sides. Build no parallel mechanism for either.
> 3. **Execute phased stop gates:** Phase 1 (G0-A) is a regression test of framework behavior the audit already confirmed, across multiple faces and QPs, in serial and MPI, before SCZM integration.
> 4. **Exact host mathematics:** In §4.2, use `symmetric_part = t * Σ_m Σ_l normal_tangent(m) * C(i,m,l,k) * gp(l)` matching host `SCZMInterfaceKernelSmallStrain.C`. Bound nonlinear constitutive scope to displacement-dependent stress with other inputs prescribed or frozen.
> 5. **Surgical migration:** Migrate from `KokkosSCZMComputeLinearElasticBulkStress` to NEML2, updating Action validation, property handles, input files, and negative tests while **preserving existing working `no_shifted = true` behavior and its gold**, dropping only the unused `bulk_stress` block from its Kokkos fixture.
> 6. **Formatting and SQA standards:** Run the mandatory formatting command `pre-commit run clang-format --files <modified-files>` prior to committing, and confirm it reports no diff. Ensure every new or migrated test specification includes explicit `requirement`, `design`, and `issues` metadata, and gates NEML2-consuming tests on `capabilities = 'kokkos & libtorch & neml2'`.
