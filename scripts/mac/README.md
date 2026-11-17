# MOOSE-SCZM macOS CLI

Use one entry point for setup, compilation, simulation runs, and tests on Apple Silicon:

```bash
./scripts/mac/moose.sh --help
```

## Common commands

```bash
# Inspect the environment without changing it
./scripts/mac/moose.sh doctor

# Dashboard: built executables, NEML2 linkage, Kokkos, progress checklist, & next-step advice
./scripts/mac/moose.sh dummy

# Install missing dependencies, build repo-isolated NEML2, and configure MOOSE
# (Installs into framework/contrib/neml2/build/installed; Conda is never modified)
./scripts/mac/moose.sh init
# Force reset framework/contrib/neml2 to MOOSE's committed SHA and build locally
./scripts/mac/moose.sh init --use-committed-neml2
# Use Conda's installed NEML2 instead of repo-isolated NEML2
./scripts/mac/moose.sh init --use-conda-neml2

# Compile one target (solid_mechanics is the default)
# Automatically syncs NEML2 using smart tiered detection (skip/python-only/fast-cxx/full)
./scripts/mac/moose.sh compile
./scripts/mac/moose.sh compile neml2
./scripts/mac/moose.sh compile solid_mechanics --skip-neml2-sync
./scripts/mac/moose.sh compile solid_mechanics --use-committed-neml2
./scripts/mac/moose.sh compile solid_mechanics --use-conda-neml2
./scripts/mac/moose.sh compile shifted_boundary_method -j 8
./scripts/mac/moose.sh compile all
# Stream raw compiler output without summary capture
./scripts/mac/moose.sh compile solid_mechanics --raw

# Run any input with the selected application
./scripts/mac/moose.sh run shifted_boundary_method path/to/input.i
./scripts/mac/moose.sh run solid_mechanics path/to/input.i --n-threads=4

# Test an existing build (regression tests; does not compile)
./scripts/mac/moose.sh test sczm
./scripts/mac/moose.sh test solid_mechanics crystal_plasticity
./scripts/mac/moose.sh test neml2 -j 8
./scripts/mac/moose.sh test all

# Run C++ GoogleTest unit tests (default target: framework)
./scripts/mac/moose.sh unit
./scripts/mac/moose.sh unit RankTwoTensor*
./scripts/mac/moose.sh unit solid_mechanics Tensor*
./scripts/mac/moose.sh compile unit

# Compile the required target, then test it
./scripts/mac/moose.sh verify sczm
./scripts/mac/moose.sh verify unit
./scripts/mac/moose.sh verify neml2-aoti

# Complete pipeline: init, compile all, test all
./scripts/mac/moose.sh all -j 8
```

Build targets are `framework`, `solid_mechanics`, `shifted_boundary_method`, `unit`, `neml2`, and `all`.
Test scopes are `framework-kokkos`, `sczm`, `neml2-aoti`, `neml2`, `all`, or any module (e.g. `solid_mechanics`).

The CLI activates `${MOOSE_CONDA_ENV:-moose}` automatically. You can override `METHOD`,
`BUILD_JOBS`, `JOBS`, or `MOOSE_CONDA_ENV` in the environment.

## Results and concurrent commands

Each TestHarness invocation and GoogleTest run prints a final line:

```text
RESULT scope=neml2 passed=8 failed=0 skipped=2 status=PASS exit=0 process_exit=0 reason=complete
```

Failures also produce `FAILED scope=... test=...` lines. Missing summaries and empty
selections return nonzero; an empty selection prints scope hints. A process error
remains an error even if all reported tests passed: `status=ERROR` and `process_exit`
preserve the distinction. In particular, exit 129 is never silently converted to
success. Parser errors also prevent `PASS`. Counts are `unknown` when no final
summary is available. A scope that runs serial and distributed variants prints a
result per invocation and stops at the first failure.

Compilation, initialization, tests, unit tests, runs, and formatting take an exclusive
checkout lock. A conflicting command fails immediately with the owning PID and
operation. The OS releases the lock on process exit; the metadata file stays in
the Git directory and does not require stale-file removal. Commands started outside
the harness do not participate in this lock.

Default build and test job counts are both **3**, stored in `scripts/mac/harness.conf`.
Environment variables and explicit `-j N` override the defaults. Rerun only failed tests:

```bash
./scripts/mac/moose.sh test neml2 --failed
./scripts/mac/moose.sh help --agent
```

`--failed` forwards to TestHarness's `--failed-tests` and needs previous results in
the selected test directory.

## Formatting

```bash
./scripts/mac/moose.sh format --check
./scripts/mac/moose.sh format next
```

The formatter must be `$CONDA_PREFIX/bin/clang-format` version 21; another version
is an error. Commands run from the target repository root and require its
`.clang-format`. Only changed `.C`, `.K`, and `.h` files are selected, with dependency
exclusions matching pre-commit. The default base is `HEAD`; staged, unstaged, and
untracked files are included. `--check` reports formatting errors without editing.
This command does not change the repository's existing pre-commit version pin.

## Other worktrees and baselines

Use the absolute path of the current harness with `-C` to target a checkout that
does not contain the harness. The caller's working directory does not select the
target repository:

```bash
/path/to/main/scripts/mac/moose.sh -C /path/to/old-worktree doctor
/path/to/main/scripts/mac/moose.sh -C /path/to/old-worktree test neml2
./scripts/mac/moose.sh baseline next neml2
```

`baseline <ref> <scope> [verify options...]` creates a detached temporary worktree,
then compiles and tests it through the current harness. It reuses the selected
Conda environment and the main checkout's installed NEML2 without syncing or
rebuilding NEML2. The baseline therefore compares MOOSE revisions against current
installed dependencies. The main checkout's initialized `large_media` supplies
the recorded test-data revision through a local clone; other dependency sources
are unnecessary for the supported Conda targets. Historical refs must support
the selected target and installed dependency APIs.

The command prints the revision, dependency path, and worktree location. It retains
the worktree and build/test outputs on success or failure for inspection. Remove
it with `git worktree remove --force <printed-path>` when finished; Git requires
`--force` for a worktree containing an initialized submodule.

## Exodus comparison

```bash
./scripts/mac/moose.sh exodiff old.e new.e
./scripts/mac/moose.sh exodiff old.e new.e -t 1e-7
```

`exodiff` uses TestHarness's `FileTester` defaults: relative tolerance `5.5e-6`,
zero floor `1e-10`, and geometric mapping. Additional exodiff options override
these defaults. `compare` continues to report differences at zero tolerance by
default; use `compare old.e new.e --cmp criteria.cmp` for explicit comparison criteria.
