Verify the CMake build of MOOSE. Run from the repository root.

**Do NOT activate the moose conda environment** — it sets `LIBMESH_DIR`/`WASP_DIR`/`PETSC_DIR`
to its own copies and the verification would test the wrong thing.

Work through the steps below in order, stopping and reporting clearly on any failure.

---

## Step 1 — Check the build artifact

Look for the CMake output library:
- macOS: `build/framework/libmoose_framework.dylib`
- Linux: `build/framework/libmoose_framework.so`

If it does not exist, report "No CMake build found — run /build first" and stop.

---

## Step 2 — Confirm it is a valid shared library

```bash
file build/framework/libmoose_framework.dylib
```

Expected: `Mach-O 64-bit dynamically linked shared library arm64`.
Any other output (truncated file, wrong arch, static archive) is a build failure.

---

## Step 3 — Confirm it links repo-local dependencies, NOT conda versions

```bash
otool -L build/framework/libmoose_framework.dylib | grep -E 'libmesh|wasp|petsc'
```

The libmesh path **must** contain `libmesh/installed/` (repo-local).
If it shows `miniforge/envs/moose/libmesh/` instead, the build was configured while
`LIBMESH_DIR` pointed at the conda copy. Fix by deleting the build directory and
reconfiguring with the explicit paths from `/build`:

```bash
rm -rf build
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER="$CONDA_PREFIX/bin/mpicc" \
  -DCMAKE_CXX_COMPILER="$CONDA_PREFIX/bin/mpicxx" \
  -DLIBMESH_DIR="$(pwd)/libmesh/installed" \
  -DWASP_DIR="$(pwd)/framework/contrib/wasp/install"
```

---

## Step 4 — Confirm exported MOOSE symbols

```bash
nm -g build/framework/libmoose_framework.dylib \
  | grep -E '_DMCreate_Moose|MooseApp|Factory' \
  | head -10
```

Non-empty output confirms the library exports MOOSE symbols correctly.

---

## Step 5 — Note on functional tests (Phase 2 gap)

The CMake migration is at **Phase 1**: the framework shared library only.
Phase 2 — CMake targets for the test application and unit test executable — is not yet
implemented.

Running functional tests via the Python test harness (`run_tests`) requires a compiled
`moose_test-opt` binary, which currently only the Make build produces. Because the Make
build picks up `LIBMESH_DIR`/`WASP_DIR` from the environment, running it without first
unsetting those vars causes the same conda-conflict problem described above.

**Until Phase 2 lands, this command verifies library correctness only (Steps 1–4).**
End-to-end simulation tests cannot be run from the CMake build path yet.

---

## Step 6 — Report results

Summarize each check:

| Check | Result |
|---|---|
| Library exists | yes / no |
| Valid shared library (correct arch) | yes / no |
| Links repo-local libmesh (not conda) | yes / no |
| Exports MOOSE symbols | yes / no |

If all pass: "CMake build verified — framework library is correct."
If any fail: show the exact output and the recommended next step.
