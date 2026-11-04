# MOOSE CUDA CLI

This CLI wraps the **existing** `kokkos-cuda-stack/scripts/build_*.sh`
installers, keeps the currently installed `kokkos-cuda-stack/prefix/`, and
avoids the benchmark and the legacy `build_moose.sh`'s `make clean` during
everyday compilations. It does not touch your macOS scripts.

The CLI is part of the checkout; no separate installer or compatibility patch
is needed. Build and run commands require Linux with CUDA. `help`, `doctor`,
and `status` also run on macOS, where missing CUDA prerequisites are expected.

The defaults retain the CUDA machine's GCC 11 / GFortran 9, CUDA at
`/usr/local/cuda`, and Ampere 8.6 target. The checkout is located from the
script, and Miniforge defaults to `$HOME/miniforge`. Existing installed
dependencies are not moved. CUDA builds use the stack's MPI and a dedicated
NEML2 venv, without activating a Conda toolchain.

## Daily usage

```bash
./scripts/cuda/moose_cuda.sh doctor
./scripts/cuda/moose_cuda.sh status
./scripts/cuda/moose_cuda.sh init
./scripts/cuda/moose_cuda.sh compile solid_mechanics -j 8
./scripts/cuda/moose_cuda.sh compile test -j 8
./scripts/cuda/moose_cuda.sh compile neml2
./scripts/cuda/moose_cuda.sh compile solid_mechanics
./scripts/cuda/moose_cuda.sh run solid_mechanics modules/solid_mechanics/test/tests/kokkos/linear_elasticity/kokkos_material_linear_elasticity.i
./scripts/cuda/moose_cuda.sh test solid_mechanics kokkos -j 1
./scripts/cuda/moose_cuda.sh verify solid_mechanics kokkos -j 8
./scripts/cuda/moose_cuda.sh verify framework kokkos -j 8
./scripts/cuda/moose_cuda.sh benchmark
./scripts/cuda/moose_cuda.sh all -j 8
```

TestHarness uses one worker by default; `test ... -j 8` requests eight
concurrent TestHarness workers and may overload a single GPU. `verify -j 8`
uses eight jobs for compiling but one test slot by default. Use `TEST_JOBS`
to override the verification test slots. Explicit MPI and thread counts raise
the slot count to at least their product, as required by TestHarness.

The wrapper accepts `--compute-device=cpu` as a direct argument after
`run ... <input>` or `test ...`, plus TestHarness flags including
`-p2 --distributed-mesh`. Both `--compute-device=cpu` and
`--compute-device cpu` are accepted. Use `--` to stop wrapper option parsing.

`init` does NOT build MOOSE executables. `compile` runs `make` rather than
skipping when executable capabilities match, so changed C++ files rebuild.
The first managed compile cleans each target once to establish its recorded
identity. A changed dependency/configuration invalidates that target and causes
another clean on its next compile. Unchanged subsequent compiles are incremental.
`compile neml2` calls the existing NEML2 builder with force rebuild;
PyTorch is reused if the legacy validation accepts it. The next MOOSE
`compile solid_mechanics` will rebuild/relink source as determined by make.
`compile framework` builds the framework library; `compile test` builds
`test/moose_test-opt`. `verify framework` builds that test executable before
running the framework TestHarness. `compile all` builds `test` and
`solid_mechanics`; the `all` command initializes the stack, builds
`solid_mechanics`, and runs its tests.

## Configuration

```
MOOSE_JOBS=8
TEST_JOBS=1
NEML2_SUPPORT=1
CUDA_DIR=/usr/local/cuda
CUDA_ARCH=86
TORCH_CUDA_ARCH_LIST=8.6
MINIFORGE_BIN=$HOME/miniforge/bin
OpenBLAS_HOME=$HOME/miniforge/envs/moose
OMPI_CC=/usr/bin/gcc-11
OMPI_CXX=/usr/bin/g++-11
OMPI_FC=/usr/bin/gfortran-9
MOOSE_CUDA_PREFIX=<checkout>/kokkos-cuda-stack/prefix
METHOD=opt
```

Set these as environment variables when overriding defaults; the CLI does
not persist user preferences. Build identities are recorded separately
under `kokkos-cuda-stack/.cuda-cli/`. `MINIFORGE_PYTHON` can override the
interpreter used to create the NEML2 venv. `FORTRAN_LIB_DIR`, `BLAS_LIB`, and
`LAPACK_LIB` override the PETSc recipe's system library locations.

To build without NEML2, retain the setting for subsequent commands:

```bash
export NEML2_SUPPORT=0
./scripts/cuda/moose_cuda.sh init
./scripts/cuda/moose_cuda.sh compile solid_mechanics
```

`init --no-neml2` and `all --no-neml2` disable it for that invocation only.
To enable it again, set `NEML2_SUPPORT=1` and run `init`. Existing initialized
submodule checkouts, including local changes, are left untouched. Use
`compile neml2` after changing its source; an import check alone does not
establish that those changes were installed.

`doctor` and `status` inspect files and processes only. They do not install,
configure, switch submodules, or mutate sources. Build logs go to
`kokkos-cuda-stack/logs/cli-*.log`.

## Dependency and configuration changes

- On the first `init`, existing valid installations are adopted as a baseline
  without claiming their original source provenance. Subsequent checks compare
  source revisions/edits, installer recipes, toolchain settings, installed file
  identities, and upstream dependency records. An upstream rebuild invalidates
  every later installer, including NEML2. Interrupted rebuilds leave downstream
  records stale so the next `init` resumes without silently reusing them.
- For a deliberate full dependency rebuild, run `init --force`. PyTorch
  is still reused when the NEML2 installer's CUDA/header/ABI checks pass.
- Configuration reuse requires matching resolved NEML2/Torch library paths
  and dependency identities. Regeneration preserves other configure options.
  Once recorded, changing the compiler, CUDA target, or prefix is rejected;
  use a separate checkout/install prefix for that toolchain. Do not copy the
  `.cuda-cli` records between checkouts. Runtime commands reject stale target
  records and ask for an appropriate compile.
- NEML2 `compile` **forces the NEML2 install step**, but legacy
  `build_neml2.sh` still uses PyTorch header/ABI checks to decide whether
  to rebuild the much larger PyTorch tree.
- Capabilities validation is implemented for executables that `make`
  produces; a successful `framework` make alone has no matching app binary
  until `test` or a module is built.

## Portable flow checks

```bash
python3 scripts/cuda/tests/test_moose_cuda.py
```

These standard-library unit tests copy the CLI into temporary mock stacks.
They exercise dependency invalidation, old library paths, stale executable
rejection, incremental builds, failure propagation, and legacy script behavior
without compiling MOOSE, installing packages, or invoking GPU tools.
