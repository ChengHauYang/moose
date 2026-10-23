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
If the repository root contains `temp_conda_env`, its first nonempty, noncomment
line overrides `MOOSE_CONDA_ENV` for all commands. Remove the file to restore
the normal environment selection.
