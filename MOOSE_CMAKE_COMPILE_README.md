# CMake Compile Command README

This file is a quick reference for compiling different parts of MOOSE with the
in-progress CMake build. For setup details, see `MOOSE_CMAKE_BUILD_GUIDE.md`.

All commands below run from the repository root.

Use the MOOSE conda environment first on `PATH`:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH <command>
```

The verified local build directory for this migration is:

```text
build-cmake-review
```

## Configure

Configure or refresh the build tree:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake -S . -B build-cmake-review
```

Use a different build type:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake -S . -B build-cmake-review -DCMAKE_BUILD_TYPE=Debug
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake -S . -B build-cmake-review -DCMAKE_BUILD_TYPE=Release
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake -S . -B build-cmake-review -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

CMake build type mapping:

| CMake build type | Make `METHOD` equivalent |
|---|---|
| `Release` | `opt` |
| `Debug` | `dbg` |
| `RelWithDebInfo` | `devel` |

## Compile Core Framework Pieces

Build the whole framework library:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_framework -j 8
```

Build only bundled parser/regex support libraries:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_pcre -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_hit -j 8
```

Build bundled GoogleTest support:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_gtest -j 8
```

Expected framework output on macOS:

```text
build-cmake-review/framework/libmoose_framework.dylib
```

## Compile Test App

Build the framework test app library and executable:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_test_exe -j 8
```

Expected outputs on macOS:

```text
build-cmake-review/test/libmoose_test.dylib
build-cmake-review/test/moose_test
```

Run the verified smoke test:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ctest --test-dir build-cmake-review -R moose_test_diffusion --output-on-failure
```

## Compile Unit Tests

Build the unit test executable:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_unit -j 8
```

Expected output:

```text
build-cmake-review/unit/moose_unit
```

Run the full registered unit CTest:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ctest --test-dir build-cmake-review -R '^moose_unit$' --output-on-failure
```

Run a focused GoogleTest filter directly:

```bash
cd unit
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ../build-cmake-review/unit/moose_unit --gtest_filter='MooseServerTest.CompletionDocumentRootLevel'
```

## Compile Individual Modules

Build a single module by target name:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target misc -j 8
```

Useful verified module targets:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target misc -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target fluid_properties -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target heat_transfer -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target solid_mechanics -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target navier_stokes -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target thermal_hydraulics -j 8
```

These have already been verified in `build-cmake-review`:

```text
misc
fluid_properties
heat_transfer
solid_mechanics
navier_stokes
thermal_hydraulics
```

Dependencies built or reused during those module builds:

```text
ray_tracing
rdg
solid_properties
```

Expected module outputs are shared libraries under:

```text
build-cmake-review/modules/lib<module_name>.dylib
```

Example:

```text
build-cmake-review/modules/libthermal_hydraulics.dylib
```

## Registered Module Targets

The current CMake module graph registers these module targets:

```text
chemical_reactions
contact
electromagnetics
external_petsc_solver
fluid_properties
fsi
functional_expansion_tools
geochemistry
heat_transfer
level_set
misc
navier_stokes
optimization
peridynamics
phase_field
porous_flow
ray_tracing
rdg
reactor
scalar_transport
solid_mechanics
solid_properties
stochastic_tools
thermal_hydraulics
xfem
```

Not currently registered as CMake module targets:

```text
combined
doc
module_loader
richards
shifted_boundary_method
subchannel
tensor_mechanics
```

## Compile Everything

Build every registered CMake target:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target all -j 8
```

Use this after representative module targets pass.

## List Available Build Targets

Ask the generated build system for available targets:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target help
```

## Test Discovery And CTest

List registered CTest tests:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ctest --test-dir build-cmake-review -N
```

Run all registered CTest tests:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ctest --test-dir build-cmake-review --output-on-failure
```

Run one CTest by regex:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ctest --test-dir build-cmake-review -R moose_test_diffusion --output-on-failure
```

## Notes

- The CMake build currently produces plain target names, not Make-style
  `*-opt`, `*-dbg`, or `*-devel` executable names.
- Linker warnings about duplicate rpaths, deprecated `-ld64`, and compact
  unwind information from MPI/PETSc/MUMPS have been observed on successful
  builds and are not currently blocking.
- In restricted sandboxes, runtime tests that initialize PETSc/OpenMPI may fail
  even when compilation is correct.
