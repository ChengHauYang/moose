# CUDA checks after rebasing onto `next` (2026-11-16)

Local-only. Drop this commit once every item below passes on the GPU box.

This file covers only what changed since the last CUDA run. For the earlier qualification work see
`.plans/sczm/handoverToCuda.md` and `.plans/tl-sczm/handoverToCuda.md`. Their SHAs are stale, and
their `git pull --ff-only` step no longer works (see step 1).

## Expected refs

| Ref | SHA |
| --- | --- |
| `neml2-v3-migration-rebase` (onto) | `3f3c15b1c8` |
| `neml2-v3-kokkos-solid-mechanics-LD-mac` | this commit, on top of `72a8299424` (33 commits on onto) |
| onto base on `next` | `c7e9e61ffa` (Merge PR #34410) |
| `framework/contrib/neml2` | `12f3d599fa` (unchanged) |

## 1. Sync: both branches were force-pushed

`git pull --ff-only` fails because both branches were force-pushed. Save any local work first, then
reset to the remote:

```bash
git fetch origin
git status --short          # commit or stash anything that matters first
git switch neml2-v3-kokkos-solid-mechanics-LD-mac
git reset --hard origin/neml2-v3-kokkos-solid-mechanics-LD-mac
git submodule status        # libmesh, petsc, wasp, neml2, pytorch, mfem: no bumps from next
```

`kokkos-cuda-stack/prefix`, `logs/`, and `.cuda-cli/` are untracked build output, and
`reset --hard` leaves them alone. Do not run `git clean -x`.

## 2. Build

`next` bumped no submodule, so the PETSc/libMesh/NEML2 stack does not need rebuilding. Only MOOSE
needs a recompile. Kokkos base headers changed, so expect most Kokkos objects to rebuild.

```bash
./scripts/cuda/moose_cuda.sh doctor
./scripts/cuda/moose_cuda.sh status        # run `compile neml2` only if this reports NEML2 stale
./scripts/cuda/moose_cuda.sh compile test -j 8
./scripts/cuda/moose_cuda.sh compile solid_mechanics -j 8
./scripts/cuda/moose_cuda.sh compile shifted_boundary_method -j 8
```

## 3. What changed and needs CUDA coverage

1. **From `next` (11 commits):** #34370, f504bdb48a on `next`, precomputes the Kokkos AuxKernel
   element mass-matrix factors into a device `JaggedArray`. It touches `KokkosAuxKernel`,
   `KokkosCopyValueAux`, `KokkosJaggedArray`, `KokkosUtils`, and `KokkosVariableValue`. It also
   switches every Kokkos mesh array to `copyToDevice`. Any Kokkos AuxKernel on our CUDA paths now
   runs through this new code.
2. **Rebase conflict in `framework/src/kokkos/mesh/KokkosMesh.K`, in 85a8db5605
   (`[kokkos_interface_materials]`):** resolved to `next`'s `copyToDevice`. Our ghost-element node
   caching is kept. This matters for interface materials on ghost neighbors, so run with
   `-p2 --distributed-mesh`.
3. **NEML2 domain design, in c79c9dffd1:** adds `boundary = ANY_DOMAIN_BOUNDARY_ID` and
   `interface = ANY_BOUNDARY_ID`, with owner-side rules. The logic runs on the host, but run the
   framework `neml2` tests against the CUDA libtorch build anyway.
4. **`bilinear_mixed_moose` `abs_zero = 1e-7`, in 3f3c15b1c8:** this tolerance was chosen from
   macOS results. Confirm it on Linux BLAS/Torch.
5. **29 benchmark tests that have never run** (`compute_devices = cuda`; they skip on the Mac):
   - SM, 9: `kokkos/{perfect_plasticity_3d,perfect_plasticity_2d,crystal_plasticity_ld}`, each with
     `host_neml2_cuda`, `kokkos_neml2_cuda`, and `kokkos_full_gpu`.
   - SBM, 20: `kokkos_sczm_{plasticity_ld,oblique_interface,tl_plasticity}`, each with
     `host_neml2_cuda`, `kokkos_neml2_cuda`, `kokkos_aijkokkos`, and `kokkos_mathypre`, plus
     `kokkos_sczm_grain_boundaries` with those four variants for both the plain and `fbar` cases.

## 4. Test order (stop at the first failure)

The harness defaults to `--compute-device=cuda`. A `compute_devices = cpu` test reported as SKIP is
expected there, so repeat each scope with `--compute-device=cpu`. A SKIP on a `*_cuda`,
`*_full_gpu`, `*_aijkokkos`, or `*_mathypre` test means the test did not run, so it does not count
as a pass.

```bash
M=./scripts/cuda/moose_cuda.sh

# A. Framework (items 1-3)
$M test framework '(neml2|kokkos)' -j 1
$M test framework '(neml2|kokkos)' -j 1 --compute-device=cpu
$M test framework 'kokkos' -j 2 -p2 --distributed-mesh

# B. Item 4
$M test solid_mechanics 'traction_separation_law' -j 1 --compute-device=cpu

# C. SM benchmarks (item 5)
$M test solid_mechanics 'kokkos/(perfect_plasticity_3d|perfect_plasticity_2d|crystal_plasticity_ld)' -j 1
$M test solid_mechanics 'kokkos/(perfect_plasticity_3d|perfect_plasticity_2d|crystal_plasticity_ld)' -j 1 --compute-device=cpu

# D. SBM benchmarks (item 5)
$M test shifted_boundary_method 'kokkos_sczm_(plasticity_ld|oblique_interface|tl_plasticity|grain_boundaries)' -j 1
$M test shifted_boundary_method 'kokkos_sczm_(plasticity_ld|oblique_interface|tl_plasticity|grain_boundaries)' -j 1 --compute-device=cpu

# E. Regression sweep of the existing Kokkos suites
$M test solid_mechanics 'kokkos' -j 1
$M test shifted_boundary_method '(kokkos_shifted_interface|shifted_cohesive_zone)' -j 1
$M test shifted_boundary_method 'sczm_pk2_kokkos_jacobian|sczm_stateful_distributed' -j 2 -p2 --distributed-mesh
```

Finally, take one Nsight profile each for an SM benchmark (`crystal_plasticity_ld`
`kokkos_neml2_cuda`) and an SBM benchmark (`kokkos_sczm_tl_plasticity` `kokkos_neml2_cuda`). Follow
the `nsys` recipe in `.plans/sczm/handoverToCuda.md`. Both Kokkos kernels and NEML2/PyTorch CUDA
kernels must appear.

## 5. Hints

- **Golds:** every CUDA variant's gold is a symlink to the CPU `*_host_out.e` gold, compared through
  the directory's `.cmp` file. Never regold from a CUDA run. If a CUDA result falls outside the
  `.cmp` tolerance, find the cause first: device math or layout versus solver path. Do not widen
  the tolerance to hide it.
- **`*_mathypre`:** needs PETSc built with hypre+CUDA (`-vec_type cuda -mat_type hypre`). An
  "unknown type" error is a stack configuration problem, not a code bug. The
  `-hypre_umpire_device_pool_size 256` pool can run out of memory on a small GPU; record that
  rather than editing the test.
- **`*_aijkokkos` / `*_full_gpu`:** with a non-CUDA-aware MPI, multi-rank runs fail with MPI error 76.
  These tests are serial, so they should not hit it.
- **Illegal address in an AuxKernel or at mesh setup:** look at item 1 first. The mass factors are
  built in `setupMassFactors()`, called from `initialSetup()`, `meshChanged()`, and
  `meshDisplaced()`. Displaced-mesh TL paths recompute them every step.
- **Failure only with `-p2 --distributed-mesh` on interface materials:** look at item 2. Ghost
  neighbor nodes and connectivity are cached in `Mesh::initMap` and `Mesh::initElement`.
- **Jacobian mismatch:** never loosen `PetscJacobianTester` tolerances (see
  `.plans/sczm/handoverToCuda.md`, "Failure triage").

## Record when done

Record the MOOSE SHA, NEML2 SHA, GPU model, driver version, and CUDA version, the pass/skip counts
for each block in section 4, and the Nsight kernel summaries under `cuda-qualification/`. Then drop
this commit (`git rebase -i neml2-v3-migration-rebase`).
