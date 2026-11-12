# SCZM Kokkos work: historical status record

> **CUDA/Linux continuation:** stop here and read
> [`handoverToCuda.md`](handoverToCuda.md). It is the single authoritative handoff for the current
> NVIDIA qualification work. The priorities and SHAs below describe an older snapshot.

> **HISTORICAL TOP PRIORITY:** Rebuild and verify the rebased branch in conda environment `moose`, starting
> with the two-rank linear `KokkosDiffusion` reproducer. The generic distributed Kokkos
> assembled-Jacobian/residual inconsistency is fixed by the upstream `next` change and is present in
> this branch's ancestry. Then resume distributed stateful SCZM certification. Keep GPU work deferred.

This file documents the core small-strain SCZM non-AD completion status. The active plan for
integrating NEML2 PK2 outputs with the Kokkos SCZM directional correction is recorded in
[`pk2-to-pk1-interface-conversion.md`](pk2-to-pk1-interface-conversion.md). That plan preserves the
converter calculation and its two primary/neighbor objects, while having the SCZM Action create them
from one user-facing Physics configuration. The separate Automatic Differentiation (AD) Kokkos SCZM
track is recorded in [`kokkos_ad_sczm_plan.md`](kokkos_ad_sczm_plan.md). Files in `archive/` are
history and may contain stale instructions.

## Current snapshot

- Date: 2026-10-16
- Branch: `neml2-v3-kokkos-solid-mechanics-LD-mac`
- HEAD: `1f23abc10e` (`Add Kokkos SCZM volumetric locking correction`)
- Rebased branch: `neml2-v3-kokkos-solid-mechanics-LD-mac`
- Rebase target: `origin/neml2-v3-kokkos-solid-mechanics-LD` at `e443cf60bb`; HEAD is based on this
  target and `0b91258033` is an ancestor.
- Safety branch: `backup-before-next-rebase-20261016` points to the pre-rebase HEAD `0fd58e3f1b`.
- Conda is established: use environment `moose` for builds, tests, and executables.
- The new pre-rebase safety stash (`pre-rebase LD-mac onto origin LD 2026-10-16`) was applied without
  conflicts and remains in the stash list.
- Post-rebase builds of `framework`, `shifted_boundary_method`, and `solid_mechanics` were executed via
  the macOS harness and verified clean.
- The two-rank `KokkosDiffusion` reproducer with `--distributed-mesh` passed with `-snes_test_jacobian`
  error ~ 8.24e-17 and converged in 1 Newton iteration to ~ 1.76e-18 / 1.22e-18.
- `kokkos/interfacekernels/regressions.neighbor_material_ghost` passed on 2 ranks distributed mesh.
- Full SCZM suite passed: 63/63 on 1 MPI rank, 24/24 on 2 MPI ranks distributed mesh.
- Full `framework-kokkos` suite passed: 14/14 on 1 MPI rank, 14/14 on 2 MPI ranks distributed mesh.
- `neml2-aoti` passed: 6/6 tests.
- `manage_state` test chain (`manage_state_advance`, `manage_state_residual_only`, `manage_state_rollback`,
  `manage_state_mesh_change`) in `solid_mechanics` passed: 4/4 tests.
- `sczm_stateful_common.i` was configured with `-pc_factor_mat_solver_type mumps` to ensure consistent
  LU solver factorization across serial and distributed execution.
- Stale `sczm_stateful_kokkos_matches_host` was renamed to `sczm_stateful_serial` with updated requirement;
  both `sczm_stateful_serial` and `sczm_stateful_distributed` pass with `rel_err = 1e-8, abs_zero = 1e-8` (zero difference).
- Numerical yield investigation: with current loading 0.01 and interface stiffness 10, stress reaches ~1.0,
  below `yield_stress = 100`, retaining linear elasticity; load 1.5 crosses yield at t=0.25 and retains plastic history.
- Changed C++ files were formatted with `clang-format -i` and pass `git diff --check`.
- `.plans/` is intentionally ignored; do not force-add it unless explicitly requested.

## Rebase completed: upstream fix is in ancestry

### Resolution on MOOSE `next`

The generic distributed Kokkos defect is resolved on MOOSE `next`. It was verified at `next` commit
`0b91258033`; the relevant upstream change appears to be `56165a0a41` (`Avoid O(p) loops over all
ranks in Kokkos setup`). That change replaces repeated rank-wise scatter setup with
`TIMPI::push_parallel_vector_data()` in the distributed Kokkos DOF/sparsity setup.

The same two-rank linear `KokkosDiffusion` reproducer that failed on the old branch passes on
`next`:

- `-snes_test_jacobian` relative error: `8.23974e-17`.
- `-snes_test_jacobian` absolute error: `7.13054e-16`.
- Timestep 1 final nonlinear residual: `1.759152e-18`.
- Timestep 2 final nonlinear residual: `1.223781e-18`.

This reproducer passed on `next` before the rebase. The rebased branch now includes the upstream fix
in its ancestry; rerun it against a fresh build before treating this branch as certified.

The minimal reproducer is:

- `/private/var/folders/9l/dhxn3q9n12z63pgxgf5nttg40000gq/T/opencode/kokkos_diffusion_distributed.i`

A reduced interface reproducer is:

- `/private/var/folders/9l/dhxn3q9n12z63pgxgf5nttg40000gq/T/opencode/sczm_elastic_transient.i`

Direct executable `-p 2` does not launch MPI. Use `mpirun -n 2` or TestHarness `-p2`.

### Local changes that may need to be reverted

The WIP stash contained an unbuilt and unverified edit in
`framework/src/kokkos/systems/KokkosSystem.K::System::setupSparsity()` that asserted:

1. PETSc local ghost indices follow `_dof_map.get_send_list()` order.
2. Each received ghost-row CSR column range is sorted.

These assertions are not part of the upstream fix and have been dropped from the working tree.
During the rebase, prefer the `next` implementation in overlapping distributed setup code and do
not restore them unless they are temporarily useful for conflict validation.

The current working tree has no diagnostic diff in `KokkosSystem.K`; the temporary assertions are
absent. The independent SCZM/NEML2 changes remain applied.

The checked-in source has already been restored from an earlier explicit-COO-row diagnostic, but the
current binaries still reflect that earlier diagnostic build. Also revert/drop any remaining
explicit-COO-row instrumentation or other speculative Kokkos assembly diagnostics. Do not revert the
SCZM, PK1 tangent, NEML2 lifecycle, state-remapping, or test changes merely because they are
uncommitted.

### Hypotheses already excluded

Do not repeat these experiments without new evidence:

- PETSc ghost refresh.
- Matrix handle replacement.
- Missing `MatZeroEntries()`.
- Combined residual/Jacobian assembly.
- Dirichlet `preset` behavior.
- Ghost-row ordering as inferred only from implicit COO row construction.
- Replacing implicit rows with explicitly stored COO row indices; this did not change either failure
  and was reverted.
- SCZM, interface-kernel, material, or NEML2-specific behavior.

PETSc source inspection confirmed that `MatSetValuesCOO()` accepts values in the caller's original
COO order and applies its saved permutation internally.

### Rebase outcome and next verification

The branch was rebased onto `origin/neml2-v3-kokkos-solid-mechanics-LD`; the pre-rebase HEAD is
preserved by the safety branch above. The old NEML2 v3 migration commit and a memory-study commit
were skipped because their changes were already upstream. Conflict resolution kept the target's
newer diagnostic script and framework threading setup while retaining the SCZM interface additions
and required PETSc solver change. The saved feature work was restored with `git stash apply`.

1. Activate conda environment `moose`, run `./scripts/mac/doctor_mac.sh`, and rebuild the narrow
   framework and shifted-boundary targets with the repository macOS harness.
2. Run the reduced two-rank diffusion reproducer with `--distributed-mesh`, first with
   `-snes_test_jacobian` and then as the original two-timestep solve. Confirm that the assembled
   Jacobian agrees with finite differences and both timesteps converge.
3. Verify the rebased branch with:
   - `kokkos/interfacekernels/regressions.neighbor_material_ghost`
   - `kokkos_shifted_interface.sczm_stateful_distributed`
4. Resume the remaining SCZM closeout only after the distributed stateful trajectory passes.

The existing distributed `kokkos/interfacekernels/regressions.neighbor_material_ghost` Jacobian test
still passes with relative error `2.04131e-09`. Account for why that test does not expose the defect.

## Implemented feature work

### PK1 tangent parity

Kokkos SCZM now supports `tangent_definition = pk1_wrt_deformation_gradient` using the host
`SCZMInterfaceKernelSmallStrainTempl` contract:

- Host-equivalent `auto` tangent inference.
- Full `dP/dF` layout and Jacobian contraction.
- `origin_part` for PK1 tangents.
- `0.5 * (origin_part + symmetric_part)` for strain tangents.
- Full rank-two/rank-four side and neighbor NEML2 output transfer through generic
  `NEML2ToKokkosMaterialProperty` aliases.
- Removal of the Kokkos PK1 action guard and corresponding documentation update.

The PK1 tests deliberately use nonsymmetric `P = A F B`; symmetric elasticity cannot distinguish the
contraction paths. Inputs with a custom `tangent` explicitly set
`stress_wrt_strain`, preserving host `auto` inference.

Passed before the current generic distributed investigation:

- Host and Kokkos PK1 Jacobian tests with nonsymmetric full `dP/dF`.
- `sczm_linear_jacobian`.
- Angled 2D and 3D host/Kokkos parity.
- `sczm_hetero_kokkos_matches_host`, including `-p2 --distributed-mesh`.

### Stateful NEML2 lifecycle

The executor now makes committed/pending NEML2 tensors, batch maps, timestep, and committed status
restartable using existing `DataIO` support for double `torch::Tensor`. Recovery preserves recovered
state, and timestep-zero initialized state is inserted into NEML2.

State ownership/remapping now includes:

- `NEML2BatchIndexGenerator::StateBatchIndexMap`.
- `getStateBatchIndexMap()`.
- `(elem, side)` keys for interface batches.
- Updated `TestNEML2ModelExecutor::remapState()` expectations for `ElemSide` keys.

`output_variables` selects published outputs when `auto_output = true`; an empty list retains
publish-all behavior. Internal state is retained regardless. The stateful SCZM case publishes only
`sczm_stress` and its tangent and reuses the existing NEML2 radial-return model through
`R2ToSR2`/`SR2ToR2` conversions.

Passed lifecycle evidence:

- `neml2_input_gatherer.exact_kinematics`.
- `neml2_input_gatherer.exact_kinematics_rollback`.
- Existing crystal-plasticity exact-kinematics coverage for repeated nonlinear evaluation, rollback,
  recovery, and state advance.
- Eight-timestep host and serial Kokkos loading/unloading runs.
- Host stateful MPI completion.

Serial and distributed stateful Exodiff specifications exist, and
`gold/sczm_stateful_kokkos_out.e` was generated. Distributed Kokkos trajectory certification now
needs the post-rebase build and verification listed above.

## SCZM closeout after the rebase: completed status

1. **Renamed stale `sczm_stateful_kokkos_matches_host`**:
   - Renamed test to `sczm_stateful_serial` in `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/tests`.
   - Updated requirement: `'The Kokkos shifted cohesive-zone formulation shall execute a stateful NEML2 radial-return loading and unloading trajectory in serial while advancing history only after converged timesteps.'`.
2. **Serial and distributed stateful Exodiff without tolerance workarounds**:
   - Set `petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'` and `petsc_options_value = 'lu mumps'` in `sczm_stateful_common.i`.
   - Regenerated `gold/sczm_stateful_kokkos_out.e` with MUMPS.
   - Verified that both `sczm_stateful_serial` (serial) and `sczm_stateful_distributed` (2-rank distributed mesh) pass with `abs_zero = 1e-8, rel_err = 1e-8` with 0.00000e+00 difference.
3. **Numerical yield and plastic history confirmation**:
   - In `sczm_stateful_common.i` with the current loading ($y = \text{'0 0.01 0.002'}$), the directional correction bulk stress is $O(1.0)$, well below `radial_return_neml2.i`'s `yield_stress = 100`. The test currently remains elastic (unloading displacement ratio is exactly $0.200000$).
   - When loading is scaled (e.g. $y = \text{'0 1.5 0.3'}$), yield occurs at $t = 0.25$, undergoing multi-iteration Newton plasticity, and retains plastic history upon unloading (ratio is $-1.15$, leaving $-5.56 \times 10^{-4}$ residual displacement).
4. **Focused Jacobian-block and volumetric-locking regressions**:
   - All tests in `test_mac.sh sczm` passed (63/63 on 1 MPI rank, 24/24 on 2 MPI ranks distributed mesh), including `sczm_linear_jacobian`, `sczm_pk1_host_jacobian`, `sczm_pk1_kokkos_jacobian`, `sczm_hetero_jacobian_blocks`, `sczm_volumetric_locking_sensitivity`, and volumetric locking regressions.
5. **Diff and code cleanliness**:
   - No diagnostic leaks, extraneous files, or untracked debris remain (`go` was not present).
   - Changed C++ files were formatted with `clang-format -i` and passed `git diff --check`.
6. **Plan updated**:
   - All verification steps recorded. GPU work remains deferred.

## Relevant files

- `framework/src/kokkos/systems/KokkosSystem.K`: upstream distributed sparsity setup and local
  temporary ordering assertions that should normally be dropped during the rebase.
- `framework/include/kokkos/systems/KokkosSystem.h`: sparsity and DOF mapping API.
- `framework/src/kokkos/systems/KokkosMatrix.K`: COO preallocation and `MatSetValuesCOO()` assembly.
- `framework/include/kokkos/systems/KokkosMatrix.h`: CSR slot lookup used by kernel accumulation.
- `framework/src/kokkos/systems/KokkosVector.K`: PETSc local/ghost vector wrapping and assembly.
- `framework/src/kokkos/systems/KokkosNonlinearSystemBase.K`: residual/Jacobian synchronization and
  assembly lifecycle.
- `framework/include/kokkos/base/KokkosResidualObject.h`: generic contribution accumulation and
  boundary-condition row suppression.
- `framework/include/neml2/materials/NEML2ToKokkosMaterialProperty.h` and
  `framework/src/kokkos/materials/NEML2ToKokkosMaterialProperty.K`: full tensor aliases.
- `framework/src/neml2/actions/NEML2Action.C` and `NEML2ActionCommon.C`: retriever selection and
  `output_variables` handling.
- `framework/include/neml2/userobjects/NEML2BatchIndexGenerator.h` and matching source: interface
  state ownership map.
- `framework/include/neml2/userobjects/NEML2ModelExecutor.h` and matching source: restartable state,
  commit/rollback, recovery, and remapping.
- `modules/shifted_boundary_method/include/kokkos/interfacekernels/KokkosSCZMInterfaceKernelSmallStrain.h`
  and matching source: tangent semantics and Jacobian contraction.
- `modules/shifted_boundary_method/src/actions/ShiftedCohesiveZoneAction.C`: Kokkos action support.
- `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/tests`: PK1 and stateful
  serial/distributed specifications.
- `modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/sczm_stateful_common.i`:
  shared loading/unloading problem.

## Verification and environment notes

- Follow the root `AGENTS.md` macOS harness map; do not invent build commands.
- Conda environment: `moose`.
- Installed `moose-dev` warned that `2026.08.23` is present while `2026.09.10` is required, although
  the requested framework and shifted-boundary targets linked successfully. Keep this visible when
  interpreting build/runtime failures.
- Rebuild after the rebase because current binaries do not exactly match restored source and do not
  contain the verified `next` implementation.
- Do not loosen tolerances or regold to hide discrepancies.
- Keep GPU work deferred. CPU Kokkos success does not certify CUDA/XPU/HIP execution or residency.
- Do not push, open a PR, or publish review comments without explicit authorization.

## Minimal prompt for the next AI session

> Read `.plans/sczm/see_this_is_enough.md` as the only active SCZM plan. Branch
> `neml2-v3-kokkos-solid-mechanics-LD-mac` has been rebased onto
> `origin/neml2-v3-kokkos-solid-mechanics-LD`; HEAD `1f23abc10e` includes `0b91258033` in its
> ancestry. The pre-rebase state is preserved in `backup-before-next-rebase-20261016`, and the
> pre-rebase feature changes were restored from the new safety stash with `apply` (the stash remains
> available). The worktree intentionally has 18 modified tracked files and 9 untracked SCZM test
> files. Do not apply another stash blindly. No build or tests have been run after the rebase. In
> conda environment `moose`, run the macOS doctor/build harness, then verify the two-rank diffusion
> Jacobian and two-timestep solve before resuming distributed stateful SCZM certification. Keep GPU
> work deferred.
