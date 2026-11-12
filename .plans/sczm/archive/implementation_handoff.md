# SCZM Kokkos implementation handoff

Snapshot: 2026-10-12. Repository: `/Users/chenghau.yang/Documents/Package/moose`.
Branch: `sczm_dev_gpu`. HEAD at handoff: `23d8bc67cd`.

This snapshot was refreshed against the source tree, commit history, test specifications, and retained
TestHarness JSON on 2026-10-12. The only path reported by `git status --short` was the untracked
`T/tests/kokkos/interfacekernels/regressions/older_state_observable.i`; `.plans/` is ignored by the
HEAD `.gitignore` change and therefore does not appear in status.

This is an **execution guide**, not a claim that the remaining work has passed tests. It
supersedes the completion claims in the [architecture plan](kokkos_sczm_plan.md) wherever the two
disagree. Re-run `git status --short` and `git log -8 --oneline` before editing: future commits may
have changed this snapshot.

Path conventions used below:

- `F` = `framework/`
- `M` = `modules/shifted_boundary_method/`
- `T` = `test/`
- `regressions/` = `T/tests/kokkos/interfacekernels/regressions/`

---

## 1. Start here

1. Read root `AGENTS.md` and any applicable nested guidance. Read the local `codegraph`,
   `verify-changes`, and `run-tests` skills when doing their respective tasks.
2. Run `git status --short` and `git log -8 --oneline`. Preserve unrelated/user WIP.
3. **Environment gate (blocking).** Before any MOOSE build or executable invocation, confirm
   whether the user's MOOSE stack uses conda and which environment.
   **The user has not answered that question in this session.** Conda paths appearing in old
   harness logs and the hardcoded `moose` environment inside `scripts/check_test_build.sh` are
   *not* confirmation. Do not use that script to bypass the gate.
4. Do **Task C next**. Task A is complete. Task B's source repair is committed, but its focused
   ordinary-neighbor dispatch acceptance remains open; do not rewrite the fix unless a reproducer
   exposes a defect.
5. After C, close B's coverage gap or re-scope it with evidence. Then harden the already-landed Phase
   5c property chain before Action integration; do not reimplement D/E from the obsolete plan text.

The branch already contains the commits listed in §2. **No permission to push or open a PR has been
given.** Before any new build or executable run, re-confirm the conda answer for the new session as
required by root `AGENTS.md`; historical use of conda `moose` is evidence, not current authorization.

---

## 2. Committed slices

| # | Commit | Scope |
| --- | --- | --- |
| 1 | `294312e64a` | Generic Kokkos interface assembly primitives |
| 2 | `c71d21b16e` | Generic interface kernel infrastructure |
| 3 | `efef10258d` | Interface dispatch/assembly, neighbor properties, ghost data |
| 4 | `f26b6ea82d` | Shifted geometry/kinematics and thin SCZM traction-jump kernel |
| 5 | `f86ea502a9` | Skip assembly-side neighbor registration when no ghost ID exists |
| 6 | `119cec4d16` | Shared face cache, known-neighbor datums, generic `InterfaceMaterial`, scalar/stateful test materials, checkpoint/restart tests |
| 7 | `1f90377a16` | Kokkos SCZM source page and truthful Physics syntax scope note |
| 8 | `eb401438c0` | Reject `constant_on != NONE`, RunException regression, generic material docs |
| 9 | `76dc6cc212` | Task A: multi-side boundary enumeration + one-side-per-face validation |
| 10 | `5d8558cbc5` | Ordinary Kokkos neighbor-material ghost datum repair and a two-rank interface regression |
| 11 | `a52971472d` | Production Kokkos shifted-jump, elastic-law, and global-traction property chain |
| 12 | `23d8bc67cd` | Ignore local planning/tool artifacts; no runtime implementation change |

Phase status against HEAD:

| Phase | State |
| --- | --- |
| 1-4 | Implemented; focused CPU tests exist |
| 5a | Implemented (traction/tangent consumer kernel + assembly); focused CPU test exists |
| 5b | Core implemented. A is complete; B's repair is committed but direct ordinary-neighbor acceptance remains incomplete; C older/rejected-step coverage remains open |
| 5c | **Implemented in source** by `a52971472d`: shifted jump/rotation, production pure-elastic law, and global traction/tangent. Only a combined serial CPU Jacobian result is retained; property-by-property host parity and HEAD rebuild are open |
| 5d | Not started: the Action still selects host objects and directional/locking/bulk-stress parity is absent |
| 6 | Not started: no real CUDA/XPU acceptance or end-to-end nonlinear law |

---

## 3. Working-tree inventory (uncommitted WIP)

`git status --short` after the revised Task C verification (still uncommitted):

```text
R  test/include/kokkos/materials/KokkosStatefulInterfaceMaterial.h -> KokkosTractionSeparationLawInterfaceMaterial.h
R  test/src/kokkos/materials/KokkosStatefulInterfaceMaterial.K -> KokkosTractionSeparationLawInterfaceMaterial.K
 M test/tests/kokkos/interfacekernels/regressions/tests
 M test/tests/kokkos/interfacekernels/stateful.i
 M test/tests/kokkos/interfacekernels/stateful_restart.i
?? test/tests/kokkos/interfacekernels/regressions/gold/
?? test/tests/kokkos/interfacekernels/regressions/older_state_observable.i
```

The experimental `TimeStepper::_failure_count` restartability change was removed. Both framework
files match HEAD; Task C no longer requires a framework change.

`.plans/` and local helper artifacts are ignored by `23d8bc67cd`, so status alone is not an inventory
of those files. Do not assume ignored files are approved source changes.

| File | Task | Current state | Disposition |
| --- | --- | --- | --- |
| `T/include/kokkos/materials/KokkosTractionSeparationLawInterfaceMaterial.h` (renamed from `KokkosStatefulInterfaceMaterial.h`) | C | Adds older-state request and old/older diagnostic properties | Keep. Renamed so the name states the traction-separation law it implements instead of implying a generic stateful base material |
| `T/src/kokkos/materials/KokkosTractionSeparationLawInterfaceMaterial.K` (renamed from `KokkosStatefulInterfaceMaterial.K`) | C | Requests the older traction state and declares diagnostics; diagnostic properties renamed `traction_at_old_state` / `traction_at_older_state` | Keep |
| `T/tests/kokkos/interfacekernels/stateful.i`, `stateful_restart.i` | C | `type` updated to the new class name | Keep; both inputs consume only `traction` and `dtraction_djump`, not the diagnostics |
| `regressions/older_state_observable.i` (untracked) | C | Runnable forced-rejection regression with direct old/older solution fields and checkpoint output | Keep and stage explicitly after review |
| `regressions/tests` | C | Contains one SQA-backed `[older_state_observable]` Exodiff block | Keep |
| `regressions/gold/older_state_observable_out.e` (untracked) | C | Inspected four-state Exodus oracle | Keep and stage explicitly after review |
| `.plans/sczm/*.md` (ignored) | — | Local handoff and architecture documents | Keep synchronized with source; do not confuse ignored status with versioned evidence |

**Committed but still needing acceptance work:**

- `5d8558cbc5` changed ordinary `NeighborInit`/`NeighborCompute` to use the pre-recorded neighbor ID
  and guarded missing contiguous IDs. However, its `neighbor_material_ghost.i` still includes the
  generic `InterfaceMaterial` test. It proves a ghost-backed interface datum and catches crashes, but
  does not directly assert that an ordinary neighbor material's dispatch executed. Treat Task B as
  implementation landed / focused acceptance incomplete.
- `a52971472d` added the full production elastic property chain and converted `sczm_linear.i` to use
  it. Do not recreate the old Task D/E classes. The next work is validation, edge cases, docs, and
  Action integration.

**Staging discipline.** Never `git add .`. The only visible WIP belongs to C; preserve it until it is
replaced with a runnable regression. Inspect ignored files separately before any commit.

---

## 4. Retained verification evidence

Evidence lives in `T/.previous_test_results.json` (older generic-interface runs) and
`M/.previous_test_results.json` (refreshed 2026-10-12 immediately before commits `5d8558cbc5` and
`a52971472d`). All retained module runs are CPU, `--compute-device=cpu`, `mpirun -n 1`; the executable
reports source commit `76dc6cc212`, so they exercised the then-working tree but do not certify a clean
build of HEAD `23d8bc67cd`. The two post-A implementation commits need a clean HEAD rebuild and
focused rerun before their behavior is called verified.

| Test | Source commit | Result |
| --- | --- | --- |
| `penalty_jacobian` (`penalty.i`) | `119cec4d16` | OK, `‖J−Jfd‖/‖J‖ = 8.51913e-11`, `‖J−Jfd‖ = 1.46102e-08` |
| `interface_material_jacobian` (`interface_material.i`) | `119cec4d16` | OK, `2.04131e-09` / `3.5143e-08` |
| `stateful.write_checkpoint` (`stateful.i`) | `119cec4d16` | OK |
| `stateful.restart` (`stateful_restart.i`) | `119cec4d16` | OK |
| `constant_on_rejected` (`../interface_material.i` plus CLI override) | `eb401438c0`-tree | OK (RunException) |
| `multi_side_interface` | `119cec4d16` | OK, `1.90567e-09` / `3.5779e-08` — **run predates the `expected_interface_faces` assertion** |
| `neighbor_material_ghost` | pre-`5d8558cbc5` working tree on `76dc6cc212` executable | Retained OK under `-p2 --distributed-mesh`; covers a ghost-backed `InterfaceMaterial` face, not direct ordinary neighbor dispatch |
| `older_state_observable` | `23d8bc67cd` plus Task C dirty paths | CPU Kokkos baseline, recover, restep, 2-thread, 2-rank, distributed, and 2-rank distributed recover all OK; not CUDA/XPU evidence |
| `shifted_penalty_jacobian` | `76dc6cc212` executable, 2026-10-12 working tree | OK, serial CPU |
| `sczm_linear_jacobian` | pre-`a52971472d` working tree on `76dc6cc212` executable | OK, serial CPU; input uses the new three-material production chain, but a clean HEAD rerun and property-level oracle are absent |

The FD values are observations of past runs, **not** new tolerances to hardcode.

### 4.3 Task C verification (`23d8bc67cd` plus dirty paths) — 2026-10-12

Environment: conda `moose`; CPU Kokkos only (`--compute-device=cpu`). Built from `T/` with
`conda run -n moose make -j6`. Dirty paths are exactly those listed in §3. No CUDA or XPU run was
performed.

Focused `older_state_observable` results, run serially from
`T/tests/kokkos/interfacekernels/regressions` to avoid shared-output races:

| Command suffix after `conda run -n moose ../../../../run_tests` | Result |
| --- | --- |
| `-j1 --re 'older_state_observable' --compute-device=cpu --no-color` | 1 passed |
| `-j1 --re 'older_state_observable' --compute-device=cpu --no-color --recover` | 2 passed (PART1 + recover) |
| `-j1 --re 'older_state_observable' --compute-device=cpu --no-color --restep` | 1 passed |
| `-j2 --re 'older_state_observable' --compute-device=cpu --no-color --n-threads=2` | 1 passed |
| `-j2 --re 'older_state_observable' --compute-device=cpu --no-color -p2` | 1 passed |
| `-j2 --re 'older_state_observable' --compute-device=cpu --no-color -p2 --distributed-mesh` | 1 passed |
| `-j2 --re 'older_state_observable' --compute-device=cpu --no-color -p2 --distributed-mesh --recover` | 2 passed (PART1 + recover) |

Existing lifecycle and failure-counter coverage:

| Working directory and command | Result |
| --- | --- |
| `T/tests/kokkos/interfacekernels`: `conda run -n moose ../../../run_tests -j1 --re 'kokkos/interfacekernels\\.stateful' --compute-device=cpu --no-color` | 2 passed |
| same with `--recover` | 4 passed |
| same with `--restep` | 2 passed |
| `T/tests/postprocessors/num_failed_timesteps`: `conda run -n moose ../../../run_tests -j1 --re 'postprocessors/num_failed_timesteps' --no-color` | 2 passed |

Gold inspection: times are `0, 0.1, 0.1999999, 0.2999999`; `failed_steps` is `0, 0, 1, 1`.
At the first accepted post-retry observation, representative `old_state` is `-1.6947656e-2` while
`older_state` is `0`; at the next observation they are approximately `-1.6436614e-2` and
`-1.6947656e-2`. Thus old and older are nonzero and directly distinguishable. Three accepted steps
are sufficient for both observations. Because TestHarness halves `num_steps = 3` to one step in the
recovery PART1 run, the forced step-2 rejection occurs after recovery starts; the final cumulative
count is therefore observed without requiring `TimeStepper::_failure_count` to be restartable.
The near-unity failure cutback keeps baseline and `--test-restep` accepted times within Exodiff's
existing tolerance so that the comparison measures rollback state rather than different schedules.

Negative controls were run against the final gold and intentionally failed Exodiff:

- replacing `getKokkosMaterialPropertyOlderByName` with the old-state accessor changed
  `older_state` at the first distinguishing observation from `0` to about `-1.6947656e-2`;
- `--cli-args 'Problem/fail_steps=99'` changed `failed_steps` from `1` to `0` at the last three
  observations.

The correct source was restored, rebuilt, and the serial baseline passed again. The prior
experimental restartable-counter change was removed and both `TimeStepper` files match HEAD. The
revised three-step test passed serial recovery and two-rank distributed recovery without that
framework change. One baseline run overlapped a recovery invocation and reported `TIME FILE MISSING`
because both harnesses shared output files; it is invalid evidence, not a product failure, and the
baseline was rerun alone successfully. `git diff --check` passed.

### 4.0 Task A verification (`76dc6cc212`) — re-recorded 2026-10-11

Environment: conda `moose`, CPU, `--compute-device=cpu`. Built `F/` and `T/`, then `M/`.
Task B's two framework files were `git stash`ed for these runs so task A was verified against a
tree without the known-bad B WIP, and restored afterwards.

| Command (from `T/`) | Result |
| --- | --- |
| `./run_tests -j4 --re 'kokkos/interfacekernels'` | 8 passed, 1 skipped, 0 failed |
| `./run_tests -j4 --re 'kokkos/interfacekernels' --n-threads=4` | 8 passed, 1 skipped, 0 failed |
| `./run_tests -j8 --re 'kokkos/interfacekernels' -p2` | 8 passed, 1 skipped, 0 failed |
| `./run_tests -j4 --re 'kokkos/interfacekernels' -p2 --distributed-mesh` | 8 passed, 1 skipped, 0 failed |
| `./run_tests -j4 --re 'kokkos/interfacekernels\.stateful' --recover` | 4 passed |
| `./run_tests -j4 --re 'kokkos/interfacekernels\.stateful' --restep` | 2 passed |
| `./run_tests -j8 --re 'kokkos/interfacekernels\.stateful' -p2 --distributed-mesh --recover` | 4 passed |
| `./run_tests -j6 --re 'kokkos'` | 183 passed, 3 skipped, 0 failed (KokkosAssembly blast radius) |
| `./run_tests -j6` (from `M/`) | 113 passed, 0 failed |

FD results at `eb401438c0`+A (source `76dc6cc212` working tree):

| Test | Result |
| --- | --- |
| `multi_side_interface` | `‖J−Jfd‖/‖J‖ = 1.90567e-09`, `‖J−Jfd‖ = 3.5779e-08` |
| `interface_material_jacobian` | `2.04131e-09` / `3.5143e-08` (unchanged) |

**Negative control for P4.** Setting `expected_interface_faces = 3` makes the run fail in both
serial and `-p2` with `Expected 3 interface faces, but cached 4.` The count assertion is live, not
vacuous. Also established by counting: the pre-fix `sideWithBoundaryID` path could only cache 2 of
the 4 faces, so the old `multi_side_interface` Jacobian was consistent but truncated — exactly the
failure mode P4 warns about.

### 4.1 Environment and build commands

Activate the confirmed environment in *each* build/test shell. For a non-conda stack use the
user's established setup instead. First build in `T/`:

```sh
make -j6          # reduce -j for available memory
```

Then run from `T/`:

```sh
./run_tests -j4 --re 'kokkos/interfacekernels' --compute-device=cpu
./run_tests -j4 --re 'kokkos/interfacekernels' -p2 --distributed-mesh --compute-device=cpu
./run_tests -j4 --re 'kokkos/interfacekernels' --n-threads=4 --compute-device=cpu
```

Confirm flags against the local `./run_tests -h`. Per AGENTS.md §7, `-j` must be at least the
requested `-p`; for MPI plus threads use at least their product (`-p2` with `--n-threads=4`
needs `-j8`).

When module code changes, also build and test in `M/`:

```sh
make -j6
./run_tests -j4 --re 'kokkos_shifted_interface' --compute-device=cpu
```

### 4.2 Recording rules

For every future run, record in this file: exact command, working directory, source commit **plus
dirty paths**, backend, process count, thread count, pass/fail/skip, and relevant numerics.
Never update golds or loosen tolerances to hide a failure. CPU Kokkos is not GPU verification:
`compute_devices = 'cpu cuda xpu'` in a test spec declares a *capability*, not a device run.
Documentation commits need their own docs-build evidence.

---

## 5. Implementation order

```text
A  multi-side boundary enumeration                         DONE  76dc6cc212
B  ordinary neighbor-material ghost repair                 LANDED 5d8558cbc5
   └─ direct NeighborInit/NeighborCompute acceptance        OPEN
C  direct older-state / rejected-step Exodiff              NEXT
D/E production jump + rotation + elastic/global traction   LANDED a52971472d
   └─ clean-HEAD rerun, property oracle, dimensions/docs    OPEN
F  ShiftedCohesiveZoneAction Kokkos selection              NOT STARTED
G  host formulation parity                                 NOT STARTED
H  nonlinear/stateful laws + real CUDA/XPU acceptance      NOT STARTED
```

**Recommended next sequence:**

1. Finish C as one focused test-material/regression slice.
2. Rebuild HEAD and re-run generic interface plus `kokkos_shifted_interface`; record exact evidence.
3. Add a direct ordinary-neighbor-material reproducer for B. If it exposes no defect, keep
   `5d8558cbc5` and close only the coverage gap; do not churn the implementation.
4. Validate the landed production chain property-by-property against the host oracle, covering
   1D/2D/3D, oblique/reversed normals, mapped QPs, base names, and dependency ordering.
5. Only then implement F, followed by G and H.

---

## 6. Task A — finish the multi-side boundary enumeration — **DONE (`76dc6cc212`)**

**Goal.** A primary element may have several sides carrying the same interface boundary ID.
`MooseMesh::sideWithBoundaryID(elem, boundary)` returns exactly one, so the face cache and the
material QP allocation silently dropped the element's other interface faces.

**What was committed.**

| File | Change |
| --- | --- |
| `F/src/kokkos/base/KokkosInterfaceFaceCache.K` | Enumerate every side carrying the boundary; keep the `(elem, side)` visited set, QP correspondence and ownership rules. **Plus** a new rejection of a boundary set on both sides of one face |
| `F/src/kokkos/base/KokkosAssembly.K` | Same enumeration in `initQuadrature()`; keeps the ghost-ID guard from `f86ea502a9`; `sid`/`eid` hoisted out of the new side loop |
| `T/src/kokkos/materials/KokkosLinearInterfaceMaterial.K` | `expected_interface_faces` assertion, plus `require_ghost_neighbor` (default `true`) so an input whose mesh keeps every interface local can opt out |
| `regressions/multi_side_interface.i` | Two central bottom elements in the primary block, two interface sides each, `expected_interface_faces = 4` |
| `regressions/tests` — `[double_marked_interface]` block | RunException on `../interface_material.i` with `cli_args` adding the flipped sideset generator |

**Decision recorded.** The both-sides case is **rejected, not deduped**: a silently chosen primary
side would decide the orientation of the face normal and the side the interface material reads, so
the user is told to mark only one side. Verified this is the existing convention — every
`SideSetsBetweenSubdomainsGenerator` in these tests sets `primary_block`, and that renames
`included_subdomains`, so only one block's sides are marked. Only a boundary marked twice (e.g. two
generators sharing one `new_boundary`) reaches the new error.

**Verification.** See §4.0. All modes green; the negative control proves the count assertion fires.

**Still out of scope for A.** Commit `5d8558cbc5` subsequently repaired ordinary neighbor datum
construction on ghost elements, but ordinary side/neighbor restriction setup still uses
`sideWithBoundaryID` and lacks all-side acceptance. That remaining scope is tracked under B.

## 7. Task B — ordinary neighbor-material dispatch — **IMPLEMENTATION LANDED; ACCEPTANCE OPEN**

Commit `5d8558cbc5` changed ordinary `Material::NeighborInit` and `NeighborCompute` to construct the
neighbor datum with the neighbor element ID pre-recorded in `KokkosMesh`, avoiding a runtime topology
lookup on a ghost element. `initializeMaterialRestrictable()` now skips neighbors without an
addressable contiguous Kokkos ID. Preserve this repair unless a focused reproducer shows it is wrong.

**Coverage gap.** The committed `regressions/neighbor_material_ghost.i` includes
`../interface_material.i`, whose material is registered with `registerKokkosInterfaceMaterial` and
uses `InterfaceMaterial::InterfaceCompute`, not ordinary `Material::NeighborCompute`. Its two-rank
distributed test is useful ghost-interface coverage, but the requirement text overstates what it
proves.

**Remaining work.**

1. Find an existing ordinary Kokkos side/neighbor material and a consumer of neighbor-side material
   storage; extend one minimally rather than creating an SCZM-specific surrogate.
2. Make the test assert that `NeighborInit`/`NeighborCompute` executes on a one-layer ghost under
   `-p2 --distributed-mesh`. The assertion must fail if repartitioning removes that path.
3. Exercise a missing/unaddressable ghost mapping and local-neighbor baseline without changing FV or
   lookup-based `MeshDatum::neighbor()` semantics.
4. Audit the still-single-side `sideWithBoundaryID` uses in ordinary material restriction setup and
   `KokkosBoundaryRestrictable`; either add all-side coverage and repair them, or explicitly split
   that behavior into a follow-up if it is independent of ghost dispatch.
5. Correct the committed regression's requirement if it remains interface-only. Rebuild and run the
   focused two-rank test plus Kokkos material/interface blast radius.

**Acceptance.** Direct ordinary-neighbor dispatch is observed on a ghost-backed datum; no ghost
runtime topology lookup occurs; missing mappings are handled before dereference; all matching sides
are retained where the restriction contract requires them. Until then, cite B as landed source, not
fully verified capability.

---

## 8. Task C — direct older-state and rejected-step acceptance

**Goal.** `KokkosTractionSeparationLawInterfaceMaterial` (renamed from
`KokkosStatefulInterfaceMaterial` during this slice, because the old name implied a generic stateful
base material while the class implements a concrete traction-separation law) currently declares current
and old traction only, with
`traction = traction_old + stiffness * jump`. It validates current/old persistence, but neither
requests the older accepted state nor proves that a rejected trial is excluded from accepted
history.

**State semantics already traced.** `FEProblemBase::advanceState()` calls Kokkos material-property
`shift()` at the start of a new step. The swap sequence moves current to old and old to older.
During step `n`, current is the trial value, old is accepted `n-1`, and older is accepted `n-2`.
`TimeStepper::rejectStep()` calls `FEProblemBase::restoreSolutions()` before retrying the same step;
it does not use older as the rollback value for current. The test must therefore prove that old and
older remain accepted history and that the failed current trial is not advanced into either one.
Delete the placeholder claim that rejection "restores from older rather than old."

**Files.** The expected minimal slice is:

- `T/include/kokkos/materials/KokkosTractionSeparationLawInterfaceMaterial.h`
- `T/src/kokkos/materials/KokkosTractionSeparationLawInterfaceMaterial.K`
- `regressions/older_state_observable.i`
- `regressions/tests` — `[older_state_observable]`
- `regressions/gold/older_state_observable_out.e`

Add a focused test-only consumer only if the output-path investigation below proves one is needed.
Do not add a production output framework for this regression alone.

**Known pitfalls.**

- P13 — old and older are successive **accepted** states; older is not a rejected-step rollback
  buffer. At an observed time, the input must produce different old and older values, or swapping
  them would leave the comparison insensitive.
- P14 — use the existing deterministic `FailingProblem` with `fail_steps = 2`; do not manufacture
  nonlinear divergence or invent a rollback engine. Include `NumFailedTimeSteps` in the compared
  output so a run that silently stops rejecting cannot pass.
- P15 — a run without forced rejection and without output comparison does not close this task. A
  SKIP is not a pass, and merely requesting older without making its value observable is not direct
  coverage.
- P20 — do not assume `KokkosMaterialRealAux` can read an interface property. It dispatches over
  element-volume `AssemblyDatum`, while this property is in boundary/interface storage. First find
  and validate an existing boundary/interface-property output or sampling path. If none exists,
  prefer a minimal test-only diagnostic consumed through the existing interface path; do not create
  generic production infrastructure for one test.
- P21 — a freshly generated gold is not evidence by itself. Inspect the compared variables and time
  sequence, and run negative controls before accepting it.

**Implementation steps.**

1. Design and record an explicit expected sequence for current, old, and older. Force step 2 to
   fail only after step 1 has been accepted, and ensure at least one output time has `old != older`.
2. Extend `KokkosTractionSeparationLawInterfaceMaterial` minimally to request traction older with
   `getKokkosMaterialPropertyOlderByName<Real>("traction")`. Expose an older-dependent diagnostic
   without changing the production state lifecycle or cohesive semantics.
3. Investigate existing boundary/interface property consumers before choosing the diagnostic output
   path. Reuse one if it addresses the correct storage. Otherwise add the smallest focused test-only
   consumer or make the diagnostic affect an existing interface-consumed solution quantity.
4. Replace the placeholder input with a runnable transient: supply the required `stiffness`, use
   `FailingProblem` with `fail_steps = 2`, add `NumFailedTimeSteps`, and write the meaningful fields
   and diagnostic to Exodus.
5. Add the currently missing test spec as one `Exodiff` test with
   `older_state_observable_out.e`. Its SQA requirement must state both direct older-state observation
   and exclusion of rejected trial history. Keep harness modes on this same spec.
6. Inspect the Exodus time sequence before retaining the gold. Run both negative controls: reading
   old where older is expected must fail Exodiff, and removing the forced failure must fail because
   the compared failure/history evidence changes.
7. Run the focused regression and the existing stateful checkpoint/restart tests. Then apply
   `--recover` and `--restep` to the same stateful specs, plus the applicable thread/MPI/distributed
   modes; do not create mode-only test specifications.

**Acceptance.** The Exodiff result directly distinguishes older from old; the compared failed-step
count proves exactly one deterministic rejection occurred; the accepted result after retry excludes
the failed current trial; both negative controls fail for the intended reason. Existing
checkpoint/restart tests and the same specs under `--recover` and `--restep` retain the accepted
history. Record exact commands, source commit plus dirty paths, backend, process/thread counts, and
results in §4/§13 before marking C complete.

---

## 9. Phase 5c production shifted jump and rotation — **LANDED (`a52971472d`)**

### 9.1 What exists now

- `InterfaceMaterial` dispatch now supplies primary and neighbor `AssemblyDatum`, enabling gradient
  evaluation in a production material.
- `KokkosSurrogateInterfaceMapping` is the shared geometry holder used by both shifted kernels and
  materials; the old private-kernel geometry blocker is resolved.
- The existing `BoundaryShortestDistanceToSurface` registers and populates each shared
  `KokkosSurrogateInterfaceMapping`: it computes host distance vectors, true normals, and reference
  rotations once the distance data are ready, then copies those arrays to device. Kokkos materials
  and kernels retain shallow handles to the shared arrays; there is no separate
  `KokkosBoundaryShortestDistanceToSurface` object.
- `KokkosSCZMComputeDisplacementJumpSmallStrain` produces shifted global/local jumps and the total
  rotation from mapped primary/neighbor QPs.
- The primary-side shift is reused for both displacement terms, preserving the host sign convention.

Do not repeat P16-P19 as unimplemented prerequisites. Instead verify that the landed implementation
satisfies them across initialization order, repeated evaluations, multiple boundaries/base names,
and every supported dimension.

### 9.2 Host oracle and exact property contract

Read `M/src/materials/SCZMComputeDisplacementJumpBase.C`,
`SCZMComputeDisplacementJumpSmallStrain.C`, and
`modules/solid_mechanics/src/materials/cohesive_zone_model/CZMComputeDisplacementJumpBase.C`.
Read `modules/solid_mechanics/include/utils/CohesiveZoneModelTools.h` for rotation.

The host uses the **primary-side** distance `d` for both terms at matched physical QPs:

```text
jump_global[i] = u_neighbor[i] - u_primary[i]
               + (grad_u_neighbor[i] - grad_u_primary[i]) dot d
Q = computeReferenceRotation(true_normal, mesh_dimension)
jump_local = transpose(Q) * jump_global
```

- Use primary `qp` and mapped `neighbor_qp`; never assume equal QP indices.
- Do not negate the neighbor shift independently from an outward normal.
- Zero inactive components as the host does; validate the displacement count against mesh
  dimension (1-3).
- Declare device dimensional properties with the exact names `displacement_jump_global` (vector),
  `interface_displacement_jump` (vector), `czm_total_rotation` (matrix), with a consistent
  optional `base_name + "_"` prefix. Determine dimensions from the host's three-component
  representation and existing Kokkos consumers; initialize every entry including unused ones.

**Property addressing (verified).** `MaterialPropertyValueBase` resolves an index as
`datum.propertyIdx(constant_option, qp)` into `property._data[datum.subdomain()]`
(`KokkosMaterialPropertyValue.h:24`). With `constant_on = NONE`, `propertyIdx` returns
`_qp_offset + qp`, and for a side datum `_qp_offset = Assembly::getQpFaceOffset(elem, side)`
(`KokkosDatum.h:506`, `:543`). So:

- the storage is the ordinary **per-subdomain** boundary material property storage; there is no
  separate "interface face" storage;
- `getElemFacePropertyIndex(elem, side)` is the **element-constant side** index used by
  `constant_on = ELEMENT` (`KokkosDatum.h:508`, `:545`). It is not a QP offset and `qp` must never
  be added to it. `InterfaceMaterial` rejects every constant mode but `NONE` (`eb401438c0`);
- the kernel reads `_traction_global(elem_datum, qp)` with the **primary** datum even for the
  neighbor residual row, which is what makes both rows read one canonical value.

### 9.3 Rotation

Because this is static small-strain geometry, compute the reference rotation with the existing host
helper at cache setup and upload it, rather than duplicating host-only tensor utilities in device
code. Preserve the exact 1D/2D/3D convention including the difficult 3D reversed-normal case.
Check CodeGraph for reusable device math before adding helpers.

### 9.4 Remaining verification

`M/test/tests/kokkos_shifted_interface/sczm_linear.i` now instantiates the production jump, elastic,
and global-traction materials and feeds the production kernel. Its retained serial CPU Jacobian run
shows self-consistency of the combined chain, not property-level equality with the host formulation.

Add property-by-property host comparisons using affine displacement fields with nonzero gradients,
oblique normals, nonzero shifts, both orientations, and mapped neighbor QPs. Include 1D/2D/3D, two
independent base names, dependency ordering, repeated evaluations, and geometry readiness. Confirm no
per-Newton host/device geometry staging.

**Acceptance.** The landed source is accepted only when current solution values drive host-equivalent
shifted global/local jump and rotation on device and the above edge cases pass.

---

## 10. Phase 5c elastic law and global traction — **LANDED (`a52971472d`)**

`KokkosPureElasticTractionSeparation` and `KokkosCZMComputeGlobalTractionSmallStrain` now form the
production local-law/global-rotation stages. They are registered module objects, not test-kernel
physics. Their implemented contract is:

```text
K_local = diag(normal_stiffness, tangent_stiffness, tangent_stiffness)
interface_traction      = K_local * interface_displacement_jump
dinterface_traction_djump = K_local
traction_global          = Q * interface_traction
dtraction_djump_global   = Q * K_local * transpose(Q)
```

Remaining work:

- Compare every local/global property and all assembled Jacobian blocks to the corresponding host
  materials, not only finite differences of the composed Kokkos system.
- Exercise unequal normal/tangent stiffness with oblique and reversed normals so a wrong rotation is
  visible; cover base-name prefixes and multiple material chains.
- Add/verify source documentation, class descriptions, and SQA design links for the three new
  production objects.
- Rebuild and rerun at clean HEAD; retained JSON predates the final commit hash.

**Acceptance.** Pure elastic completion closes only the first production law. Nonlinear,
irreversible, and real-device acceptance remains open under H.

---

## 11. Tasks F-H

**F — Action integration.** Read `M/src/actions/ShiftedCohesiveZoneAction.C` and the parent
`modules/solid_mechanics/src/actions/CohesiveZoneAction.C`. The parent constructs materials and
kernels, so swapping just the kernel is insufficient. Follow existing Kokkos backend selection
conventions; do not invent an SCZM-only switch. Create the entire compatible material graph together
and validate property producer types. Forward boundary, displacement/component, distance UO, base
name, traction/tangent names, and supported options. Preserve host behavior and NEML2 validation;
explicitly reject unsupported features rather than silently dropping terms — in particular the host
default `directional_correction = true` must not quietly lose its contribution. Test ordinary
Physics syntax, two boundaries/base names, and invalid mixed pipelines.

**G — Host formulation parity.** Read both host `SCZMInterfaceKernelBase` and
`SCZMInterfaceKernelSmallStrain` before modifying device formulas. Provide device primary/neighbor
bulk stress and tangents first. Then port directional correction and its consistent derivatives for
`stress_wrt_strain` and `pk1_wrt_deformation_gradient`, the locking correction, and `no_shifted`.
Preserve the signed area correction (currently `true_normal · surrogate_normal`, i.e.
`trueNormal().dot_product(datum.normals(qp))`), traction signs, and QP mapping. Do not switch to
absolute value or shifted test functions based on the abstract plan — read the actual host
formulas. Implement or explicitly reject unsupported coordinate systems. Compare host and Kokkos
residual vectors and matrices at the same solution state with correction on and off; FD alone only
establishes self-consistency, not host equivalence.

**H — Nonlinear/stateful and real devices.** After elastic parity, select an existing nonlinear
cohesive law, then an irreversible/history law; port exact semantics and tangents. NEML2, if
chosen, needs an explicit device-resident interface batch contract, not merely `device = cuda`.
Verify converged fields and history, rejected steps and restart, distributed ownership, and
representative dimensional/orientation cases. Compile and run real CUDA plus another available
backend, retaining results. Profile geometry/constitutive transfers across repeated Newton
iterations. Do not optimize atomics or add coloring unless profiling identifies contention. Do not
call CPU Kokkos an end-to-end GPU result.

---

## 12. Commit and handoff discipline

Use one reviewable change per remaining slice: C; B acceptance; Phase 5c verification/docs; F; G;
H. A, B's implementation, and D/E's initial production chain are already committed; do not manufacture
new commits that merely recreate those changes. Each new slice needs meaningful tests and relevant
docs.

**Error-test convention.** Do not write a separate input file whose only purpose is to fail. Write a
normal, working test input and trigger the error from the spec with `cli_args`, as in
`test/tests/kokkos/materials/constant_on/tests`. The command line is parsed as a HIT tree and merged
into the input tree (`Parser::parse`), so `cli_args` may override a parameter *or add a section*.
Use this for the `constant_on_rejected` input too; see §14.

Before committing: inspect staged paths and diff, preserve other WIP, check SQA metadata, check
whitespace/format on changed code, and record focused build/test results. The user has authorized
ready commits but **not** arbitrary removal of WIP, push, or publication. Update §2, §3 and the log
below with exact commit hashes when a task closes. Never mark a skipped placeholder, a historical
claim, or an unrun backend as passing.

---

## 13. Continuation log

- **2026-10-11** — `1f90377a16` and `eb401438c0` committed. A's source simplification and the
  4-face assertion are present but **not built, not tested, not committed**. B and C remain WIP and
  both B files are known-incorrect as written. Conda environment question still unanswered; no MOOSE
  build/test and no docs build were run.
- **2026-10-11 (later)** — Task A implemented, built and verified in every harness mode, and
  committed as `76dc6cc212` with the conda `moose` environment confirmed. Evidence in §4.0.
  Task B's framework files were stashed during those runs and restored afterwards. Added the
  both-sides interface rejection plus its RunException regression, and `require_ghost_neighbor`
  so `multi_side_interface.i` can use a mesh whose interface is not split across ranks.
- **2026-10-12** — `5d8558cbc5` landed the ordinary neighbor-datum ghost repair and the committed
  two-rank interface regression. The regression does not directly dispatch an ordinary neighbor
  material, so B's focused acceptance remains open.
- **2026-10-12** — `a52971472d` landed the production Kokkos SCZM property chain: shared surrogate
  mapping, Kokkos distance data, shifted jump/rotation, pure-elastic local law, and global
  traction/tangent. `sczm_linear.i` uses the chain. Retained serial CPU Jacobian evidence came from
  the pre-commit working tree and does not replace a clean HEAD rebuild or host property oracle.
- **2026-10-12** — HEAD `23d8bc67cd` only changes `.gitignore`. The sole visible WIP at that point
  was the non-runnable untracked `older_state_observable.i`.
- **2026-10-12 (Task C review slice)** — Replaced the placeholder with one direct old/older Exodiff
  regression, deterministic `FailingProblem fail_steps = 2`, inspected gold, both negative controls,
  checkpoint/recover/restep, threads, MPI, and distributed coverage. The test now uses three accepted
  steps, placing the forced rejection after recovery's one-step PART1 checkpoint; the experimental
  `TimeStepper::_failure_count` restartability change was removed. Exact commands and dirty paths are
  in §3/§4.3. The slice is verified but remains uncommitted pending user review; no CUDA/XPU run occurred.
- **2026-10-12 (Task C rename slice)** — Renamed `KokkosStatefulInterfaceMaterial` to
  `KokkosTractionSeparationLawInterfaceMaterial` (via `git mv`, history preserved) and its diagnostic
  properties `traction_old_state` / `traction_older_state` to `traction_at_old_state` /
  `traction_at_older_state`. The old class name implied a generic stateful base material; the class
  implements a concrete traction-separation law. The diagnostic rename removes confusion with the
  framework's own `traction_old` state handle. No physics changed and the gold was **not** regenerated:
  Exodus nodal variable names come from the input's variable names, not property names, so the existing
  gold still matched. All Task C modes, the `stateful` series, and both negative controls were re-run
  against the renamed source in conda `moose`; all pass. Negative controls re-confirmed after the
  rename. No CUDA/XPU run. Not committed pending review.
- **Next focused task:** review and commit C. **Then:** re-verify HEAD, close B's direct-dispatch
  coverage, harden Phase 5c, and proceed to F Action integration.
- When resuming, append entries with command, working directory, source commit plus dirty paths,
  backend, `-p`/`--n-threads`, and result. Do not append prose such as "all tests pass".

---

## 14. Error-test convention — recorded and applied

Resolved by history rewrite, not a new commit. `constant_on_rejected` is now `../interface_material.i`
plus `cli_args = 'Materials/interface_material/constant_on=ELEMENT'`, and its standalone input file is
deleted. That fix was folded into its own commit (`eb401438c0`) by rebuilding the last two commits
from `1f90377a16`; a backup branch `backup/sczm_before_rebase` holds the pre-rewrite `a0f1fb4560`.

Preconditions checked before rewriting: `sczm_dev_gpu` has no upstream, and no remote branch on
`moose` or `origin` contained `045162fda7`. Do not rewrite again without re-checking that.
