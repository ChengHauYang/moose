# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repository is

MOOSE (Multiphysics Object-Oriented Simulation Environment) is a finite-element framework for
coupled multiphysics simulations built on libMesh and PETSc. It provides a plugin-based module
system and a factory-driven object registration pattern. The primary audience is simulation
engineers writing physics kernels, boundary conditions, and materials.

## Build systems

### Make (current authoritative build)

The Make system is not driven from the repository root. Entry points are subdirectories.

```bash
# Framework + test application
cd test && make -j 8 METHOD=opt

# Debug build
cd test && make -j 8 METHOD=dbg

# Unit tests
cd unit && make -j 8

# A specific module
cd modules/solid_mechanics && make -j 8
```

`METHOD` defaults to `dbg` when generating `compile_commands.json`, otherwise `opt`.
`MOOSE_JOBS ?= 8` is the parallelism default used by build scripts.

Output libraries are named `libmoose-opt.la`, `libsolid_mechanics-opt.la`, etc. (method suffix baked in).

### CMake (migration in progress — Phase 1 complete, Phase 2 not started)

Phase 1 builds `moose_framework` only. Phase 2 (modules, test, unit executables) is not yet implemented.

**Prerequisites (one-time, slow):**
```bash
unset LIBMESH_DIR
MOOSE_JOBS=6 METHODS=opt scripts/update_and_rebuild_libmesh.sh   # ~30–60 min
scripts/update_and_rebuild_wasp.sh                                 # ~5 min
```

**Configure and build:**
```bash
cmake -S . -B build -G Ninja
cmake --build build --target moose_framework
```

CMake files live in `cmake/` and `framework/CMakeLists.txt`. See `MOOSE_CMAKE_BUILD_GUIDE.md`
for the full reference. Do not use Make and CMake build trees simultaneously.

## Running tests

Tests are driven by a Python harness. Run from the directory of the application under test:

```bash
# All framework tests, 8 parallel workers
cd test && ./run_tests -j 8

# Filter by name (regex)
cd test && ./run_tests -j 8 --re diffusion

# Run tests from a specific spec file
cd test && ./run_tests --spec-file tests/kernels/tests

# Module tests
cd modules/solid_mechanics && ./run_tests -j 8
```

`scripts/run_tests` is the harness entry point when not inside a build directory.
Test specs use HIT format (`tests` files) and reference gold files for comparison.

## Code formatting

```bash
# Format a file in-place
clang-format -i path/to/file.C

# Check only
clang-format --dry-run --Werror path/to/file.C
```

Style: LLVM base, 100-column limit, 2-space indent, Allman braces, no tab characters.
Config is in `.clang-format` at the repository root.

## Architecture

### Object registration and factory system

All physics objects (Kernels, BCs, Materials, etc.) follow a single pattern:

1. **Declaration** — class inherits from a base (e.g., `Kernel`, `Material`) and defines
   `static InputParameters validParams()`.
2. **Registration** — `registerMooseObject("AppName", ClassName)` in a `.C` file triggers
   static initialization that inserts a builder into `Factory`.
3. **Instantiation** — during input file parsing, `Factory::create()` looks up the registered
   type by string name and constructs the object with validated `InputParameters`.

Registration happens at library load time (static initializers). The macro expands to a dummy
global variable whose constructor calls `Registry::add<T>()`. This means registration order
within a binary depends on link order — module libraries must link in dependency order.

### Input file parsing flow

1. HIT parser reads the `.i` input file into a tree.
2. `ActionFactory` instantiates `Action` objects for each block.
3. `ActionWarehouse` executes actions in task-dependency order.
4. Each action calls `Factory::create()` to populate warehouses (e.g., `KernelWarehouse`).
5. The executioner then drives the nonlinear solve over assembled objects.

### Module dependency system

Modules are Make-level libraries. Dependencies are declared in each module's `Makefile` via
`DEPEND_MODULES`. `modules/modules.mk` processes these into link-ordered library lists. For a 
detailed map of module-to-module dependencies, see `modules/README.md`.

When a downstream module links an upstream one, the upstream library's registered objects become
available in the combined binary.

A module's registered objects are only available if the module's `.la` (or `.so`) appears in
the link line — missing `DEPEND_MODULES` causes silent link failures or missing-type runtime errors.

### MooseApp / MooseObject hierarchy

- **`MooseBase`** → provides `_app` reference and common utilities.
- **`ParallelParamObject`** → adds MPI communicator and console access.
- **`MooseObject`** → factory-creatable base; all physics objects inherit from this.
- **`MooseApp`** → root of a simulation; owns `Factory`, `ActionFactory`, `Executioner`,
  `TheWarehouse`, and the output system.
- **`Action`** → not a `MooseObject`; configured during parsing, calls `Factory` to populate
  the simulation with objects.

Applications inherit `MooseApp`, implement `registerAll()` to register their objects and any
module objects they depend on, and implement `registerApps()` for app-level registration.
Entry point is `Moose::main<AppType>(argc, argv)`.

### Test harness

Test specs are HIT files named `tests` inside `tests/` subdirectories. Each spec block declares
a tester type (`Exodiff`, `CSVDiff`, `RunException`, etc.), an input file, and expected outputs
or behaviors. The harness compiles the test binary if needed, then runs tests in parallel,
comparing against gold files.

## Key files worth knowing

| File | Purpose |
|---|---|
| `framework/build.mk` | Compiler flags, METHOD, MOOSE_JOBS defaults |
| `framework/moose.mk` | Source directories, library name, core dependency wiring |
| `framework/app.mk` | Template included by every application and module Makefile |
| `modules/modules.mk` | Module registry, DEPEND_MODULES resolution, link ordering |
| `framework/include/base/MooseApp.h` | Root application class |
| `framework/include/base/MooseObject.h` | Base of all factory-created objects |
| `framework/include/Registry.h` | `registerMooseObject` / `registerMooseAction` macros |
| `framework/include/Factory.h` | Object creation from string names |
| `python/TestHarness/TestHarness.py` | Full test harness implementation |
| `MOOSE_MAKE_BUILD_GUIDE.md` | Make build quick reference |
| `MOOSE_CMAKE_BUILD_GUIDE.md` | CMake build quick reference (migration in progress) |
