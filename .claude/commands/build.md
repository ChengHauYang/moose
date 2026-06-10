Build MOOSE using CMake. Run from the repository root.

**Do NOT activate the moose conda environment.** Activating it exports `LIBMESH_DIR`,
`WASP_DIR`, and `PETSC_DIR` that point at conda's own copies of these libraries, which
conflict with the versions built inside this repository. Use the conda compiler binaries
directly via their absolute path instead.

---

## Locate the conda prefix (no activation needed)

```bash
CONDA_PREFIX=$(conda info --base)/envs/moose
```

Verify this directory exists before proceeding.

---

## Step 1 — Build libmesh (one-time, ~30–60 min)

Skip if `libmesh/installed/bin/libmesh-config` already exists.

The script needs MPI wrappers; run it through `conda run` but unset the conflicting
library vars inside that shell so it builds against the repo submodule, not conda's copy:

```bash
conda run -n moose bash -c '
  unset LIBMESH_DIR WASP_DIR PETSC_DIR
  MOOSE_JOBS=6 METHODS=opt scripts/update_and_rebuild_libmesh.sh
'
```

---

## Step 2 — Build WASP (one-time, ~5 min)

Skip if `framework/contrib/wasp/install/include` already exists.

```bash
conda run -n moose bash -c '
  unset LIBMESH_DIR WASP_DIR
  scripts/update_and_rebuild_wasp.sh
'
```

---

## Step 3 — Configure with CMake

Run if `build/` does not exist, or if `CMakeLists.txt` or `framework/CMakeLists.txt`
is newer than `build/CMakeCache.txt`.

The `mpicc`/`mpicxx` wrappers call an underlying clang
(`arm64-apple-darwin20.0.0-clang`) that must itself be on `PATH`. Add conda's `bin/`
to PATH first. A `-D` flag goes into the CMake cache and takes priority over any env
var, so conda's `LIBMESH_DIR`/`WASP_DIR` cannot interfere even if set in the shell:

```bash
export PATH="$CONDA_PREFIX/bin:$PATH"
unset LIBMESH_DIR WASP_DIR PETSC_DIR   # prevent env vars from overriding -D flags

cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER="$CONDA_PREFIX/bin/mpicc" \
  -DCMAKE_CXX_COMPILER="$CONDA_PREFIX/bin/mpicxx" \
  -DLIBMESH_DIR="$(pwd)/libmesh/installed" \
  -DWASP_DIR="$(pwd)/framework/contrib/wasp/install"
```

Expected output on success:
```
-- Found LibMesh: .../libmesh/installed/bin/libmesh-config (C++17)
-- Found MooseWASP: .../framework/contrib/wasp/install (...)
-- Configuring done
```

---

## Step 4 — Build the framework

Always run this step:

```bash
cmake --build build --target moose_framework -j8
```

On success the library is at `build/framework/libmoose_framework.dylib` (macOS)
or `build/framework/libmoose_framework.so` (Linux).

---

Report the result clearly: success with the output library path, or the first error
with a diagnosis and suggested fix.
