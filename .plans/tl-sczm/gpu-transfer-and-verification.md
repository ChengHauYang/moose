# TL-SCZM GPU-machine transfer and verification plan

## Goal

Transfer the uncommitted host AD/non-AD total-Lagrangian shifted cohesive zone method (TL-SCZM)
work to the CUDA machine through Git, then verify that it builds in the CUDA-Kokkos stack without
claiming that TL-SCZM itself runs on the GPU.

TL-SCZM is intentionally host-only. The CUDA-machine checks must therefore distinguish between:

- compiling the application against the CUDA-Kokkos stack;
- running the TL-SCZM host tests with `--compute-device=cpu`; and
- confirming that requesting Kokkos TL-SCZM is rejected with the intended diagnostic.

## Current state

- Working branch: `neml2-v3-kokkos-solid-mechanics-LD-mac`
- The TL-SCZM changes are uncommitted.
- macOS build passed with:

  ```bash
  ./scripts/mac/moose.sh compile shifted_boundary_method -j 3
  ```

- The focused macOS tests passed:

  ```bash
  ./scripts/mac/moose.sh test shifted_boundary_method \
    'shifted_cohesive_zone.total_lagrangian|shifted_cohesive_zone.action_error_tests.kokkos_total_lagrangian_rejected' \
    -j 3
  ```

  Result: 4 passed, 0 skipped, 0 failed.

- The broader `shifted_cohesive_zone` group previously completed all simulations, but six Exodiff
  comparisons were blocked on macOS because the local `exodiff` executable could not load
  `@rpath/libneml2_eager.dylib`.

## Success criteria

1. The intended source, documentation, input, and test files are committed without unrelated files.
2. The commit is pushed to a remote branch that the CUDA machine can fetch.
3. `./scripts/cuda/moose_cuda.sh doctor` reports a usable CUDA stack.
4. `shifted_boundary_method` compiles successfully in that stack.
5. The three host TL-SCZM tests pass on the CUDA machine with `--compute-device=cpu`:
   - AD Jacobian;
   - non-AD hand-coded Jacobian;
   - high-order basis warning.
6. The Kokkos TL-SCZM request is rejected with `Kokkos TL-SCZM is not currently implemented.`
7. `git diff --check` is clean and the small-strain SCZM interface kernel remains unchanged.
8. Any broader-suite failures are classified as implementation failures or environment/tooling
   failures; expected output is not changed merely to hide a failure.

## Phase 1: review and prepare the transfer commit on macOS

1. Review the complete working-tree diff and file list:

   ```bash
   git status --short
   git diff --check
   git diff --stat
   ```

2. Confirm there is no diff in the small-strain sibling kernel:

   ```bash
   git diff -- \
     modules/shifted_boundary_method/include/interfacekernels/SCZMInterfaceKernelSmallStrain.h \
     modules/shifted_boundary_method/src/interfacekernels/SCZMInterfaceKernelSmallStrain.C
   ```

3. Confirm the transfer contains only the intended TL-SCZM implementation:

   - host AD/non-AD TL displacement-jump material;
   - host AD/non-AD TL global-traction material;
   - host AD/non-AD TL interface kernel;
   - finite-strain Action wiring and Kokkos rejection;
   - fitted/shifted reference-normal customization;
   - regression inputs and test specifications;
   - object and Physics documentation;
   - the `StabilizationUtils.h` include guard needed for unity-build reuse.

4. Stage only those files. Inspect the staged diff before committing:

   ```bash
   git diff --cached --check
   git diff --cached --stat
   git diff --cached
   ```

5. Create one transfer commit. Suggested subject:

   ```text
   Add host total-Lagrangian shifted cohesive zones
   ```

6. Do not rewrite, squash, or force-push existing history solely for the transfer.

## Phase 2: push a fetchable branch

1. Push only after explicit approval of the staged diff and commit.
2. Prefer pushing the current branch normally if it is the intended shared development branch:

   ```bash
   git push origin neml2-v3-kokkos-solid-mechanics-LD-mac
   ```

3. If the current `-mac` branch should remain machine-specific, create and push a dedicated branch
   instead, for example:

   ```bash
   git switch -c tl-sczm-host
   git push -u origin tl-sczm-host
   ```

4. Record the exact branch name and commit SHA for the CUDA-machine checkout.

## Phase 3: fetch on the CUDA machine

Use normal non-destructive Git operations. Do not reset or discard CUDA-machine work.

1. Inspect the CUDA checkout first:

   ```bash
   git status --short --branch
   ```

2. If the checkout is clean, fetch and switch to the pushed branch:

   ```bash
   git fetch origin
   git switch <pushed-branch>
   git pull --ff-only
   ```

3. Confirm the expected commit:

   ```bash
   git log -1 --oneline
   git status --short --branch
   ```

4. If the CUDA checkout has local modifications or a conflicting branch state, stop and preserve
   that work rather than resetting it.

## Phase 4: CUDA environment and build

Run all checks through the CUDA harness; do not invent direct `make`, MPI, CUDA, PETSc, or Conda
commands.

1. Inspect the stack:

   ```bash
   ./scripts/cuda/moose_cuda.sh doctor
   ./scripts/cuda/moose_cuda.sh status
   ```

2. Build the affected module:

   ```bash
   ./scripts/cuda/moose_cuda.sh compile shifted_boundary_method -j <N>
   ```

3. A successful CUDA-stack build establishes compatibility with the CUDA-Kokkos build. It does not
   establish that TL-SCZM executes on a GPU.

## Phase 5: focused behavior checks

### Host TL-SCZM checks

Run the host implementation explicitly on the CPU:

```bash
./scripts/cuda/moose_cuda.sh test shifted_boundary_method \
  'shifted_cohesive_zone.total_lagrangian.ad_jacobian|shifted_cohesive_zone.total_lagrangian.nonad_jacobian|shifted_cohesive_zone.total_lagrangian.high_order_warning' \
  --compute-device=cpu -j 3
```

These checks establish:

- AD residual/Jacobian consistency;
- consistency of the hand-coded shifted-jump, unshifted-`F`, and directional-correction Jacobian;
- the warning for displacement basis orders above `SECOND`.

### CUDA/Kokkos rejection check

Run the rejection regression with CUDA selected:

```bash
./scripts/cuda/moose_cuda.sh test shifted_boundary_method \
  'shifted_cohesive_zone.action_error_tests.kokkos_total_lagrangian_rejected' \
  --compute-device=cuda -j 1
```

The expected result is a passing `RunException` test that matches:

```text
Kokkos TL-SCZM is not currently implemented.
```

This is the intended GPU-related behavior, not a missing implementation test failure.

## Phase 6: broader regression coverage

After the focused checks pass, run the complete shifted cohesive-zone group on the host path:

```bash
./scripts/cuda/moose_cuda.sh test shifted_boundary_method shifted_cohesive_zone \
  --compute-device=cpu -j <N>
```

For any failure, record:

- the exact test name;
- whether the application solve converged;
- whether failure occurred in execution, Jacobian comparison, `EXODIFF`, `CSVDIFF`, or another
  postprocessor;
- the relevant missing library or linkage diagnostic if it is an environment failure.

Do not loosen Jacobian tolerances, update gold files, or change expected output without identifying
an actual mathematical or intentional output change.

## Phase 7: final report

Report:

- pushed branch and commit SHA;
- CUDA harness `doctor`/`status` result;
- compile result;
- focused host-test result;
- Kokkos rejection-test result;
- broader-suite totals and any blocked comparisons;
- confirmation that no TL-SCZM GPU execution claim is being made.

If CUDA-only source or test failures appear, fix them in the smallest follow-up commit and rerun the
narrowest affected checks before repeating the broader group.
