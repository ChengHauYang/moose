# How To Compile MOOSE With CMake

This guide covers the CMake build system that is being introduced alongside the existing
Make-based build. CMake is the intended authoritative replacement. Make remains a reference
during migration.

For a command-only quick reference covering framework, test app, unit, module,
and all-target builds, see `MOOSE_CMAKE_COMPILE_README.md`.

## Prerequisites

- CMake 3.20 or later (`cmake --version`)
- Ninja (`brew install ninja` on macOS, or use `-G "Unix Makefiles"`)
- A C++17-capable compiler (AppleClang 14+, GCC 12+, Clang 14+)
- Python 3 (for the revision header generator)
- PETSc must be built or available — libmesh depends on it

## Build flow

All commands run from the repository root unless stated otherwise.

### 1. Initialize submodules

libmesh and WASP are git submodules. This step checks out their source trees
but does **not** build them — compiled output is produced in steps 2 and 3.
If you cloned with `--recurse-submodules` this step is already done; otherwise
run it once:

```bash
git submodule update --init --recursive
```

### 2. Build libmesh from the bundled submodule

libmesh uses an autoconf build system and must be built before running CMake.
The script installs to `libmesh/installed/`, which is the default `LIBMESH_DIR`
in `cmake/FindLibMesh.cmake`. It also runs `git submodule update` internally,
so steps 1 and 2 are safe to run in either order.

```bash
unset LIBMESH_DIR          # clear any environment override pointing elsewhere
export MOOSE_JOBS=6
export METHODS=opt
scripts/update_and_rebuild_libmesh.sh
```

After this step, `libmesh/installed/bin/libmesh-config` must exist.

### 3. Build WASP from the bundled submodule

WASP is a required framework dependency. The script installs to
`framework/contrib/wasp/install/`, which is the default `WASP_DIR`
in `cmake/FindMooseWASP.cmake`.

```bash
scripts/update_and_rebuild_wasp.sh
```

After this step, `framework/contrib/wasp/install/include/` must exist.

### 4. Configure

```bash
cmake -S . -B build -G Ninja
```

If your environment has `LIBMESH_DIR` or `WASP_DIR` set to non-default locations,
override them explicitly:

```bash
cmake -S . -B build -G Ninja \
    -DLIBMESH_DIR=/path/to/libmesh/installed \
    -DWASP_DIR=/path/to/wasp/install
```

Expected output on success:

```
-- Found LibMesh: .../libmesh/installed/bin/libmesh-config (C++17)
-- Found MooseWASP: .../framework/contrib/wasp/install (...)
-- Configuring done
-- Build files have been written to: .../build
```

### 5. Build the framework

```bash
cmake --build build --target moose_framework
```

This builds three targets in dependency order:

| Target | Type | Description |
|---|---|---|
| `moose_pcre` | static lib | bundled PCRE regex library |
| `moose_hit` | static lib | bundled HIT input-file parser |
| `moose_framework` | shared lib | core MOOSE framework |

The output library is `build/framework/libmoose_framework.dylib` (macOS)
or `build/framework/libmoose_framework.so` (Linux).

### 6. Verify the output (optional)

On macOS:

```bash
otool -L build/framework/libmoose_framework.dylib | head -20
```

On Linux:

```bash
ldd build/framework/libmoose_framework.so | head -20
```

You should see libmesh, PETSc, and WASP libraries in the output.

## Build types

Pass `-DCMAKE_BUILD_TYPE=` to select the build type:

| CMake type | Make METHOD equivalent | Flags |
|---|---|---|
| `Release` (default) | `opt` | `-O2 -DNDEBUG` |
| `Debug` | `dbg` | `-O0 -g` |
| `RelWithDebInfo` | `devel` | `-O2 -g` |

Example:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

## Optional features

These are off by default. Enable only if the corresponding library is available.

```bash
cmake -S . -B build -G Ninja \
    -DMOOSE_ENABLE_KOKKOS=ON   -DKOKKOS_DIR=/path/to/kokkos \
    -DMOOSE_ENABLE_LIBTORCH=ON -DLIBTORCH_DIR=/path/to/libtorch \
    -DMOOSE_ENABLE_MFEM=ON     -DMFEM_DIR=/path/to/mfem \
    -DMOOSE_ENABLE_NEML2=ON    -DNEML2_DIR=/path/to/neml2
```

## Reconfiguring after a dependency change

If libmesh or WASP is rebuilt, delete the build directory and reconfigure:

```bash
rm -rf build
cmake -S . -B build -G Ninja
```

Incremental reconfiguration after a dependency rebuild is not supported in Phase 1.

## Known differences from the Make build

| Make behavior | CMake Phase 1 behavior |
|---|---|
| Output named `libmoose-opt.la` | Output named `libmoose_framework.dylib/.so` (no method suffix) |
| Unity build (`MOOSE_UNITY=true`) | Not replicated; individual source files used |
| `oprof` method | Not mapped; use `RelWithDebInfo` as the nearest equivalent |
| Module libraries (`level_set`, etc.) | Registered as individual module targets in `modules/CMakeLists.txt` |
| Test and unit executables | Registered as `moose_test_exe` and `moose_unit` |

## Relevant files

```
CMakeLists.txt                       top-level project
cmake/FindLibMesh.cmake              locates libmesh via libmesh-config
cmake/FindMooseWASP.cmake            locates pre-built WASP
cmake/MooseOptions.cmake             MOOSE_ENABLE_* option declarations
cmake/MooseRevision.cmake            build-time MooseRevision.h regeneration
framework/CMakeLists.txt             moose_pcre, moose_hit, moose_framework
test/CMakeLists.txt                  moose_test, moose_test_exe
unit/CMakeLists.txt                  moose_unit
modules/CMakeLists.txt               module target graph
MOOSE_CMAKE_COMPILE_README.md        compile command quick reference
```
