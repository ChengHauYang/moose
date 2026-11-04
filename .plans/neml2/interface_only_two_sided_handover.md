# Handover: Host NEML2 `interface_only` two-sided output fix

Branch: `neml2-v3-kokkos-solid-mechanics-LD-mac`. Written 2026-10-26.

> **SUPERSEDED.** The block-restriction approach described below (plan Option 1, the `_neml2_interface_only`
> private flag) was abandoned, and the Tier-4 duplicate-declaration blocker it reports is resolved. The
> neighbor-side retriever is now a boundary-restricted interface material carrying
> `_material_data_type = NEIGHBOR_MATERIAL_DATA`, so the retrievers stay boundary-restricted and never
> collide with a bulk block producer. All four tiers pass; see
> `.plans/neml2/host-interface-only-two-sided-final-report.md` for the current state, and
> `.plans/neml2/host-interface-only-two-sided-output.md` (Revision 2) for the architecture analysis.
> Sections 1 (motivation), 7 (commits to reference) and 9 (do-not list) below are still accurate;
> Sections 2-6 and 8 describe the abandoned approach and are kept only as a record.

This hands off the host `interface_only = true` two-sided output work. The fix is **implemented and
passes its targeted tests, but Tier-4 (the real benchmark) exposed a genuine defect that blocks
upstream submission.** Read this before touching the fix or re-running the benchmark.

---

## 1. What the fix is for

On a `BreakMeshByBlockGenerator` cohesive interface, an `interface_only = true` NEML2 model
(`interface = Block0_Block1`) must publish its side-local constitutive outputs (PK1 stress, PK1
tangent) on BOTH the primary and neighbor sides, because the two sides are distinct subdomains with
distinct deformation gradients / plastic state. The host path today publishes only the primary side;
the Kokkos path publishes both. So the host SCZM kernel reads zero neighbor stress/tangent, omitting
the block-1-side shifted-CZM directional correction that Kokkos includes -- the ~1.8x block-1
`slip_hardening` gap between host step1 and Kokkos step3. Host is the deficient reference; Kokkos is
correct. The fix makes the ordinary `interface_only = true` API two-sided automatically, with NO new
user parameter, reusing the existing `NEML2ToMOOSEMaterialProperty<T>`.

## 2. The approach taken (plan Option 1 -- private flag)

`framework/src/neml2/actions/NEML2Action.C`, host `interface_only` branch (lines ~591-613): instead
of boundary-restricting the host retriever (`boundary = _interface`, which is never
neighbor-replicated, so it publishes only the primary side), **block-restrict** it to the two
subdomains the interface straddles, obtained via `MooseMesh::getInterfaceConnectedBlocks(BoundaryID)`.
MOOSE's `addMaterialHelper` then auto-creates a FACE copy and a NEIGHBOR copy of the block material;
the interface element loop runs the FACE copy into `_bnd_material_props` (primary) and the NEIGHBOR
copy into `_neighbor_material_props` (neighbor), each keyed on its own `ElemSide` batch index. This is
how the Kokkos path already works.

Because a block material also instantiates an inert BASE (volume) copy with `_bnd == false`, and that
copy would hit `neml2BatchIndex` (which errors in interface_only mode, where the volume batch map is
never populated), the fix adds a private flag:
- `NEML2ToMOOSEMaterialProperty.h`: `const bool _interface_only;` member.
- `NEML2ToMOOSEMaterialProperty.C`: `addPrivateParam<bool>("_neml2_interface_only", false)`, the
  initializer, and a guard at the very top of `computeProperties()`:
  `if (_interface_only && !_bnd) return;` -- makes the BASE volume copy inert while leaving the
  FACE/NEIGHBOR (`_bnd == true`) copies fully active.
- `NEML2Action.C` sets `_neml2_interface_only = true` on the host retriever params in interface_only
  mode. The Kokkos branch is unchanged (stays boundary-restricted).

## 3. Four-tier verification -- results (current binaries, 2026-10-26)

Binaries: `test/moose_test-opt` and `modules/shifted_boundary_method/shifted_boundary_method-opt`,
both rebuilt against the current framework carrying the fix. Run on `--compute-device=cpu`.

- **Tier 1 -- PASS.** `neml2.interface_only_two_sided` (CSVDiff). Two-sided material output with
  deliberately different primary/neighbor inputs; both sides publish. Test + gold are in the tree
  (see section 5).
- **Tier 2 -- PASS.** `neml2.interface_only_two_sided_jacobian` (PetscJacobianTester, `run_sim`).
  Covers element-element, element-neighbor, neighbor-element, neighbor-neighbor couplings.
- **Tier 4 -- FAILS AT SETUP. This is the blocker.** `step1_ld_sczm_host_cpu.i` errors before
  solving:

  ```
  The following material properties are declared on block 0 by multiple materials:
    slip_hardening   __neml2_to_moose_slip_hardening__bulk__
                     __neml2_to_moose_slip_hardening__interface__  ...
  ```

- **Tier 3 -- NOT REACHED.** Host/Kokkos fixed-state parity is blocked until Tier-4 runs.

## 4. The defect (root cause, confirmed from source + input)

The fix block-restricts **every** interface-only retriever to `{block0, block1}`
(`NEML2Action.C:591-613` runs for each output/derivative the interface model emits). In the
benchmark, BOTH NEML2 models emit a `slip_hardening` output:
- `[NEML2/bulk]` (crystal-plasticity bulk model) -> `__neml2_to_moose_slip_hardening__bulk__`,
  block-restricted to `{0,1}`.
- `[NEML2/interface]` (`interface_only = true`) -> `__neml2_to_moose_slip_hardening__interface__`,
  now ALSO block-restricted to `{0,1}` by the fix.

Two materials declaring the same property name on the same block -> MOOSE hard-errors at setup.

The OLD boundary-restricted interface path declared `slip_hardening` on a *boundary*, not a block, so
it never collided. Switching to block-restriction (required for two-sided host publishing)
reintroduces the collision for ANY output name shared between an interface_only model and a bulk
model on the same blocks. Tier-1/2 pass only because their minimal inputs have no coexisting bulk
producer of the same property name.

This is exactly the "coexistence with bulk producers" correctness point the plan flagged to verify.
It is realized, not hypothetical. The fix as written is NOT upstream-ready.

## 5. Design options to resolve the collision (pick one -- this is a real design decision)

The collision is structural to Option 1: block-restriction declares a block-level property, which can
clash with a bulk producer. Candidate directions, roughly increasing in reach:

1. **Disambiguate the interface retriever's declared property name.** Give the interface_only
   retriever's `to_moose` property a distinguishing suffix (e.g. `<name>_interface`) so it never
   collides with a bulk producer, then teach the SCZM consumer to read that name on both sides. Keeps
   Option 1's two-copy publishing. Cost: a naming scheme + consumer-side plumbing; must not break the
   single-model interface_only case that has no bulk model.
2. **Only block-restrict the retrievers the SCZM kernel actually consumes two-sided** (PK1 stress,
   PK1 tangent), and leave other outputs (e.g. `slip_hardening`, which the interface model emits but
   the two-sided consumer does not read per-side) boundary-restricted or suppressed. Needs a way to
   know which outputs are two-sided consumers.
3. **A publishing mechanism that does not block-declare on shared blocks** -- e.g. a dedicated
   neighbor-replicated boundary material, which is a larger framework change (new class / warehouse
   path) and crosses into the "large restructuring -- present both paths" territory of AGENTS.md s3.

Recommendation: start with option 1 or 2; both stay inside the files already being edited. Confirm
against the benchmark (Tier-4) AND a single-model interface_only input (no bulk model) so the fix does
not regress the simple case.

## 6. State of the working tree (what is and is not committed)

Committed on this branch:
- `f8d0f7b78d [framework] Regold NEML2 input-derivative output tests` -- UNRELATED to this fix. Five
  NEML2 volume-test golds (`custom_model`, `parameter`, `blocks_same_model`, `blocks_different_model`,
  `multiple_input_files`) held `dproduct/dA = 0.0`, captured against NEML2 submodule `8965e62288`
  where the input-derivative export was broken. The bump to `12f3d599fa` FIXED that export; the code
  now returns the correct `dproduct/dA = p1*p2*B`; the golds were stale. Traced end-to-end with
  instrumentation (since removed); the `0.0` in each exodiff was the gold operand, not the computed
  value. This was NOT a regression and is independent of the interface-only work.

Uncommitted, part of the interface-only fix (the `[WIP]` commit, if made):
- `framework/src/neml2/actions/NEML2Action.C` -- host interface_only block-restriction hunk
  (~lines 591-613). NOTE: this file also carries an unrelated pre-existing refactor (host_gathered
  inputs, output_initializers, scalar Real backend), so its 115-line diff is NOT all the fix.
- `framework/include/neml2/materials/NEML2ToMOOSEMaterialProperty.h` -- `_interface_only` member.
- `framework/src/neml2/materials/NEML2ToMOOSEMaterialProperty.C` -- private param, initializer, and
  the `if (_interface_only && !_bnd) return;` guard (also carries unrelated refactor hunks).
- `framework/include/neml2/userobjects/NEML2ModelExecutor.h` +
  `framework/src/neml2/userobjects/NEML2ModelExecutor.C` -- pre-existing output_initializers refactor
  that the fix depends on being present; NOT the interface-only logic itself.
- Tests (clean, issue #32413): `test/tests/neml2/interface_only_two_sided.i`,
  `interface_only_two_sided_jacobian.i`, `gold/interface_only_two_sided_out.csv`, and the two
  `interface_only_two_sided*` blocks added to `test/tests/neml2/tests`.

Uncommitted, UNRELATED to this fix (do NOT fold into the interface-only commit):
- Kokkos scalar bridge: `NEML2ToKokkosMaterialProperty.h/.K/.md`, the scalar Real support in
  `NEML2Action.C`/`NEML2ToMOOSEMaterialProperty`, `kokkos_scalar_output.i` + gold + the
  `kokkos_scalar_output` block in `tests` (issue #31596).
- `KokkosElementExtremeMaterialProperty.h/.K/.md` (new postprocessor).
- `KokkosSCZMInterfaceKernelSmallStrain.h` (Kokkos SCZM rotation fix).
- `modules/shifted_boundary_method/test/tests/kokkos/` benchmark inputs (plasticity_LD_sczm).

Because the production files mix the interface-only fix with the unrelated pre-existing refactor, a
hunk-level split is required to isolate the fix cleanly for the eventual
`[pick for neml2 bnd updateing PR]` commit. Do that split only when the Tier-4 defect is fixed and the
whole thing re-verifies.

## 7. The canonical commits to reference (BY MESSAGE TITLE, not SHA) when the fix is ready

The eventual non-WIP commit uses prefix `[pick for neml2 bnd updateing PR]` and references:
- the consolidated boundary/interface Gauss-point execution + interface_only (closes #32413)
- the test/class fixes adding the CZM tests
- the MOOSE/NEML2 traction-separation law

## 8. How to re-run the tiers

```
# Tier 1 + 2 (framework neml2):
./scripts/cuda/moose_cuda.sh test test "two_sided" --compute-device=cpu

# Tier 4 (benchmark; needs SBM module built against current framework):
./scripts/cuda/moose_cuda.sh compile shifted_boundary_method -j 8
cd modules/shifted_boundary_method/test/tests/kokkos/plasticity_LD_sczm
# host step1 vs Kokkos step3; compare avg_slip_hardening / max_slip_hardening / avg_disp_x_block1
./shifted_boundary_method-opt -i step1_ld_sczm_host_cpu.i --compute-device=cpu
./shifted_boundary_method-opt -i step3_ld_sczm_kokkos_cpu_neml2.i --compute-device=cpu
```

Source the cuda stack first (`kokkos-cuda-stack/scripts/activate.sh`) when running a binary directly
instead of through the harness.

## 9. Do-not list

- Do NOT push, force-push, or regold beyond the five already-committed volume-test golds.
- Do NOT delete `exact_kinematics_pk1_neml2.i` or any benchmark/model file.
- Do NOT fold the unrelated Kokkos-scalar / postprocessor / SCZM-rotation work into the interface-only
  commit.
- Do NOT attach `[pick for neml2 bnd updateing PR]` until the Tier-4 collision is fixed and all four
  tiers (plus a single-model interface_only input) verify on the current binaries.
