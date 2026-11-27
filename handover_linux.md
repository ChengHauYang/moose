# Linux/CUDA handover: SCZM weighted traction, 0622126e69..4b5c86aa82

The 41 commits after `0622126e69` on `frontier_working` have been built and tested only on macOS
(Apple clang, CPU, libtorch without CUDA). None of them has been compiled with GCC/nvcc or run on
a GPU. Goal: build them on the Linux CUDA box, run them on CPU and CUDA, open the device coverage
of the new Kokkos tests that are still gated to CPU, and fix any Linux- or device-only failure at
its root cause.

This file lives in a commit tagged `[drop this after linux got passed]`. Drop that commit once
the report below is green.

## State on the Mac at 4b5c86aa82

- `shifted_boundary_method` TestHarness, CPU: 295 passed, 40 skipped, 9 failed. All 9 failures are
  `kokkos_shifted_interface.weighted_traction_stabilization_cuda/*`, which fail only because the
  Mac libtorch has no CUDA ("Torch not compiled with CUDA").
- `shifted_boundary_method-unit-opt --gtest_filter='SCZM*'`: 18 passed.
- `solid_mechanics` and `framework` suites were not rerun at the tip.
- Golds regenerated on the Mac in the last commit, because the stiffness-weighted average
  traction differs from 1/2 on distorted meshes: `kokkos_shifted_interface/gold/sczm_salehani_tl_host_out.e`
  (about 5e-5 relative) and `gold/sczm_tl_fbar_recovery_kokkos_out.csv` (up to 0.6 percent).
  `gold/sczm_salehani_tl_kokkos_out.e` is a symlink to the host gold.

## 0. Get the code

```bash
git fetch origin && git switch frontier_working && git merge --ff-only origin/frontier_working
git log --oneline -2          # this handover commit, then 4b5c86aa82
git diff --stat 0622126e69 HEAD -- framework/contrib libmesh petsc .gitmodules   # empty: no pin moved
./scripts/cuda/moose_cuda.sh doctor && ./scripts/cuda/moose_cuda.sh status
```

No dependency moved, so `init` and `compile neml2` are not needed. `scripts/cuda/` and
`scripts/harness/` are untracked; if the box lacks them, copy them from the Mac checkout.

## 1. What changed, ordered by Linux/GPU risk

| Area | Commits | Why it may break on Linux or CUDA |
|---|---|---|
| Kokkos SCZM producer `KokkosSCZMStabilizationData` and the Kokkos SCZM kernels | `de9e71bc8f` `c7357706cc` `959c6c4edc` `b099ec916f` `4000cbe4ae` `9befb0a941` `c4079db351` `4b5c86aa82` | Device Robin solve, frozen beta and average-traction weight (device arrays plus restartable host copies), element-linear stress recovery, prepass failure flag |
| Native Kokkos trial laws | `c275c77d01` `38c3dd798b` `2634d316bf` `3df7b1e21b` | Coupled cubic, Salehani-Irani, stateful mixed-mode bilinear evaluated inside the device Robin loop; `recordTrialState` detected by `std::void_t` SFINAE plus `if constexpr` |
| Shared SCZM headers compiled for host and device | `de9e71bc8f` `2d5afcae8b` `a943b43cda` | `SCZM_KOKKOS_FUNCTION` macro, default arguments on device functions, 81-entry local arrays |
| Framework Kokkos | `8c8698241e` `8abcaa4738` | Interface faces and ghosted-property queries in `KokkosAssembly`, `KokkosInterfaceKernel`, `KokkosMaterialProperty*` |
| Framework NEML2 | `c67277b150` `d65ad45a18` `26269f6668` `580a35d297` `9bd9b7d827` | NEML2 models evaluated at trial jumps in resumable batch Robin steps; state advance from the auxiliary jump. With `NEML2/device=cuda` this is torch on the GPU fed from Kokkos device arrays |
| solid_mechanics | `74a97116c1` `5f226d7543` `0475b08f02` `60cfa4afac` `94ff3bbe20` `ddfdc239a9` | Kokkos PK1 tangent on residual passes, Lagrangian tangents on face evaluations, host CZM trial-jump refactors |
| Host SCZM Robin solve | `f1b746f229` `1535c3bcdd` `26854df4a8` | CPU only; GCC warnings and Linux MPI |
| Grain IDs on a distributed mesh | `38253555f7` | MPI only; run with `-p 2 --distributed-mesh` |

## 2. Build

```bash
T=./scripts/cuda/moose_cuda.sh
$T compile test -j $(nproc)
$T compile solid_mechanics -j $(nproc)
$T compile shifted_boundary_method -j $(nproc)
```

Build the SBM unit tests in `modules/shifted_boundary_method/unit` (`make -j`) in the same
environment `moose_cuda.sh` exports (see `scripts/cuda/moose_cuda_common.sh`), then run
`./shifted_boundary_method-unit-opt --gtest_filter='SCZM*'`. Expect 18 passed.

Report every new GCC or nvcc warning in the files of section 1, not only errors.

## 3. Linux CPU

`-j` counts TestHarness slots and must be at least `-p`.

```bash
$T test shifted_boundary_method -j 8 --compute-device=cpu
$T test shifted_boundary_method 'weighted_traction|grain_boundary_case|sczm_stiff_interface' -j 8 -p 2 --distributed-mesh --compute-device=cpu
$T test solid_mechanics 'cohesive_zone_model|lagrangian|kokkos' -j 8 --compute-device=cpu
$T test framework 'kokkos|neml2' -j 8 --compute-device=cpu
```

Expected: everything passes except tests restricted to CUDA, which are skipped. A gold that misses
by a small amount on Linux only is floating-point reordering between compilers: report the
measured difference against its tolerance and stop. Do not loosen the tolerance or regenerate the
gold on Linux without asking.

## 4. CUDA, tests already enabled on the device

Fastest first; stop at the first failure, fix it, rerun only that test (`--re <exact name>`).

```bash
$T test shifted_boundary_method 'coupled_cubic_rejections|stress_recovery_rejections' -j 4
$T test shifted_boundary_method 'weighted_traction_stabilization/small_strain' -j 4
$T test shifted_boundary_method 'weighted_traction_stabilization_cuda' -j 4 --timing
$T test shifted_boundary_method 'weighted_traction' -j 4 --timing
$T test shifted_boundary_method 'weighted_traction' -j 4 --timing --compute-device=cpu
$T test shifted_boundary_method 'kokkos_' -j 4
$T test solid_mechanics kokkos -j 4
$T test framework kokkos -j 4
```

`weighted_traction_stabilization_cuda` runs the 13 total-Lagrangian Kokkos tests with
`NEML2/device=cuda`; its `total_lagrangian_fbar_recovery_solution` compares against the Mac gold
`sczm_tl_fbar_recovery_kokkos_out.csv`.

## 5. CUDA, tests still gated to CPU

These groups were added in this range with `compute_devices = cpu` only because no GPU was
available, so a CUDA run skips them. They hold most of the new device code: the native trial laws,
the stateful bilinear law, NEML2 trial laws, and the grain-boundary path with the interface
manager.

| Group (`kokkos_shifted_interface` unless noted) | Host children, keep on `cpu` | Kokkos children to open on CUDA |
|---|---|---|
| `salehani_irani_weighted_traction` | `small_strain_host`, `finite_strain_host` | `small_strain`, `finite_strain`, `jacobian` |
| `bilinear_mixed_mode_weighted_traction` | `host` | `matches_host`, `finite_beta`, `jacobian_elastic`, `jacobian_softening`, `jacobian_softening_current_mixity` |
| `neml2_traction_weighted_traction` | none | all 9 |
| `bilinear_mixed_mode_neml2_weighted_traction` | none | all 3 |
| `neml2_traction_laws_weighted_traction` | none | all 6 |
| `neml2_weighted_traction_rejections` | none | all 3; check that each `expect_err` still matches on CUDA |
| `grain_boundary_case.kokkos_weighted_traction` | none | both |

Do it in two passes:

1. Kokkos on the device, NEML2 unchanged. Move `compute_devices` from the group to the children,
   setting `'cpu cuda xpu'` on the Kokkos children and `cpu` on the host children. The NEML2 blocks
   keep their device, so this exercises the device Robin solve and the Kokkos-to-NEML2 transfers.
   A Kokkos child compared against a host gold must match it within its existing tolerance.
2. NEML2 on the device. For the children whose interface traction law is a NEML2 model (the
   `*_neml2*` children and those using `sczm_bilinear_neml2.i`), add CUDA-only copies with
   `NEML2/device=cuda` in `cli_args` and the same gold and tolerances. Follow the existing
   `weighted_traction_stabilization_cuda` group. This is the separate device mode the
   TestHarness has no option for, so a separate child is justified. Here every Robin batch
   iteration evaluates the torch model on the GPU.

Commit the spec changes that pass. Report any child that fails, with the first error, and leave it
on `cpu` with a one-line comment that cites the failure.

## 6. Code to watch

Paths relative to `modules/shifted_boundary_method/`.

a. `include/utils/SCZMGeneralizedRobinSolver.h:125,147`: device functions take
   `const GeneralizedRobinSolverOptions & options = {}`. If nvcc rejects the default or warns
   about calling a host function from a host-device function, replace the default with an
   overload that forwards `GeneralizedRobinSolverOptions{}`.
b. `#undef SCZM_KOKKOS_FUNCTION` at the end of `SCZMGeneralizedRobinSolver.h`,
   `SCZMCohesiveLawEvaluators.h`, `SCZMBiLinearCohesiveLaw.h`, and `SCZMSalehaniIraniCohesiveLaw.h`.
   The host unit tests and the `.K` objects compile the same inline functions with different
   annotations. Report any duplicate-symbol or `__host__`/`__device__` mismatch at link time.
c. `include/utils/SCZMGlobalTractionTotalLagrangian.h:119-128`: three `Real[3][3][3][3]` local
   arrays per thread on the finite-strain tangent pass. Check register spills with
   `-Xptxas -v`.
d. `include/kokkos/userobjects/KokkosSCZMStabilizationData.h:283` computes and freezes beta and the
   primary-side weight on the first evaluation. `src/kokkos/userobjects/KokkosSCZMStabilizationData.K:317-318`
   copies both to the device after a restart, and `:351-362` copies them back for the restartable
   data. A CUDA-only mismatch in the `*recovery*` tests or after a cutback points here.
e. `KokkosSCZMStabilizationData.h:435`: a failed device Robin solve sets `_prepass_failure`, and
   `.K:373` turns it into `setException("Generalized SCZM Robin solve failed.")`. That message on
   CUDA but not on CPU is a finding.
f. `KokkosSCZMStabilizationData.h:441`: `if constexpr (RecordsTrialState<...>)` selects
   `recordTrialState` for stateful laws. If nvcc picks the wrong branch, the bilinear softening
   tests fail on CUDA while passing on CPU.
g. `.K:406-432`: the NEML2 trial-law path gathers the Robin inputs to the host on every batch
   step. This is correct but costly; report its share of the `--timing` wall time if the NEML2
   groups are much slower on CUDA than on CPU.

## 7. Invariants

- Never schedule a `KokkosUserObject` on `EXEC_PRE_KERNELS`. `KokkosSCZMStabilizationData` is
  `EXEC_NONE` and is driven by the kernels through `ensurePrepared()`.
- Device failure: set a flag, read it after the fence, and on the failing rank call only
  `_fe_problem.setException(msg)`. No `throw`, `::Kokkos::abort()`, `comm().max()`, or
  `checkExceptionAndStopSolve()`.
- Robin math: `S = (I + K/beta)^-1` and `W = S K`. Never `W = beta (I - S)`, which cancels digits
  when beta >> |K|.
- No CUDA special cases, no tolerance loosening, nothing under `framework/contrib/neml2*`.
- No separate tests for MPI counts, threads, mesh modes, recover, or restep; use the TestHarness
  options. A hand-coded Jacobian change needs a `PetscJacobianTester` covering it.
- Kokkos objects need no AMR or displaced-mesh code.

## 8. Fix and commit

1. Fix the root cause, rebuild incrementally, rerun the single test, then the section 3 and 4
   sweeps for the touched area on both devices.
2. Commit each verified fix on `frontier_working` as `[sbm] <what changed>` (or `[framework]`,
   `[solid_mechanics]`), stating the Linux or device cause in one or two lines, ending with the
   Co-Authored-By line.
3. Before pushing, `git fetch origin && git rebase origin/frontier_working`, because the Mac may
   push meanwhile. Never move `origin/frontier_working_temp1` (pinned at `562bff3036`).

## 9. Report back

- Build: pass or fail per target, with the full text of any error and every new warning in the
  section 1 files; whether item (a) needed the overload.
- Section 3: pass/fail/skip counts per command, and each failure with its first error line.
- Section 4: the same, plus `--timing` wall times of each `weighted_traction*` test on CPU and CUDA.
- Section 5: which children now run on CUDA, which stay on CPU and why.
- Any gold or tolerance miss as the measured difference against the current tolerance.
- The commits made, with their hashes.
