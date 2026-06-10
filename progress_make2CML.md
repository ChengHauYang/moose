# Make to CML Progress

Date checked: 2026-05-04

Guidance for future LLMs:

- Do not restart this migration from scratch. Treat everything under `Done` as
  already implemented or already verified unless a new build proves otherwise.
- Work only from `TODO`. When a TODO item is completed, move that exact item to
  `Done` and add the command or evidence that verified it.
- Keep this file in only two major sections: `Done` and `TODO`.
- Keep `build-cmake-review/` as a local build directory unless the user asks for
  a different build directory.
- Use the MOOSE conda environment at the front of `PATH` for configure, build,
  and tests:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH <command>
```

- Some commands need elevated permissions in this sandbox:
  - Building targets that run `MooseRevision.cmake`, because the revision script
    touches `.git/HEAD` and `.git/index`.
  - Runtime tests using PETSc/OpenMPI, because sandboxed `bind()` and
    `getdomainname()` calls fail.
- `CMakeLists.txt` files are currently ignored by `.gitignore` because of the
  repo-wide `*.txt` rule. Use `git add -f` for those files when staging, or add
  explicit unignore rules as a TODO item.

## Done

- Added CMake project entry points:
  - top-level `CMakeLists.txt`
  - `framework/CMakeLists.txt`
  - `test/CMakeLists.txt`
  - `unit/CMakeLists.txt`
  - `modules/CMakeLists.txt`

- Added framework CMake targets:
  - `moose_pcre`
  - `moose_hit`
  - `moose_gtest`
  - `moose_framework`

- Added test app CMake targets:
  - `moose_test`
  - `moose_test_exe`

- Added framework unit CMake target:
  - `moose_unit`

- Added module graph registration through `modules/CMakeLists.txt` and
  `moose_add_module()`.

- Added CMake helper modules:
  - `cmake/FindLibMesh.cmake`
  - `cmake/FindMooseWASP.cmake`
  - `cmake/MooseOptions.cmake`
  - `cmake/MooseRevision.cmake`
  - `cmake/MooseApplication.cmake`
  - `cmake/MooseModule.cmake`

- Implemented libmesh discovery through `libmesh-config`.
  - Produces `LibMesh::LibMesh`.
  - Falls back from stale environment `LIBMESH_DIR` to bundled
    `libmesh/installed`.

- Implemented WASP discovery.
  - Produces `WASP::WASP`.
  - Uses bundled default `framework/contrib/wasp/install`.

- Implemented CMake build options and method mapping.
  - Optional feature switches exist for Kokkos, LibTorch, MFEM, and NEML2.
  - `CMAKE_BUILD_TYPE` maps to MOOSE `METHOD`.

- Implemented `MooseRevision.h` regeneration through CMake.
  - Verified once with elevated permissions after sandbox denied touching
    `.git/HEAD`.

- Updated TestHarness executable lookup.
  - `python/TestHarness/TestHarness.py` checks for CMake-built executables in
    the build tree before falling back to Make-style `*-METHOD` names.

- Updated `unit/run_tests`.
  - It checks for CMake `../build/unit/moose_unit` before falling back to the
    Make unit executable.

- Added CMake build documentation updates.
  - `MOOSE_CMAKE_BUILD_GUIDE.md` documents submodule, libmesh, WASP, configure,
    build, and verification steps.
  - `MOOSE_MAKE_BUILD_GUIDE.md` has minor cross-reference updates.

- Added CMake compile command quick reference.
  - `MOOSE_CMAKE_COMPILE_README.md` lists how to compile the framework, bundled
    support libraries, test app, unit tests, individual modules, all targets,
    and how to list/run CTest tests.
  - `MOOSE_CMAKE_BUILD_GUIDE.md` now links to this quick reference and no
    longer says module/test/unit targets are not in the CMake graph.

- Verified CMake configure in `build-cmake-review`.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake -S . -B build-cmake-review
```

- Verified `moose_framework` builds and links.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_framework -j 8
```

Produced:

```text
build-cmake-review/framework/libmoose_framework.dylib
```

- Verified `moose_test_exe` builds and links.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_test_exe -j 8
```

Produced:

```text
build-cmake-review/test/libmoose_test.dylib
build-cmake-review/test/moose_test
```

- Fixed CTest registration for `moose_test_diffusion`.
  - Initial CTest registration used bare `moose_test`; CTest searched the
    working directory and failed to find the build-tree executable.
  - Fixed by using target-aware generator expression:

```cmake
COMMAND "$<TARGET_FILE:moose_test_exe>"
```

- Verified `moose_test_diffusion` CTest smoke test passes with elevated runtime
  permissions.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ctest --test-dir build-cmake-review -R moose_test_diffusion --output-on-failure
```

Result:

```text
1/1 tests passed
```

- Verified `moose_unit` builds and links.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target moose_unit -j 8
```

Produced:

```text
build-cmake-review/unit/moose_unit
```

- Fixed CTest registration for `moose_unit`.
  - Initial CTest registration ran the unit executable from the build tree,
    which broke data-file/current-directory assumptions in many unit tests.
  - Fixed by using the target path and unit source directory:

```cmake
COMMAND "$<TARGET_FILE:moose_unit>"
WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/unit"
```

- Fixed CMake/Make parity for disabled NEML2 support.
  - `Moose.C` always registers `NEML2ActionCommon` and `NEML2Action` syntax.
  - Make compiles `framework/src/neml2` even when `ENABLE_NEML2=false`; those
    sources self-gate external NEML2 behavior with `NEML2_ENABLED` but still
    provide disabled-mode action registration.
  - CMake previously excluded `framework/src/neml2` when
    `MOOSE_ENABLE_NEML2=OFF`, causing
    `MooseServerTest.CompletionDocumentRootLevel` to fail with
    `NEML2ActionCommon is not a registered Action`.
  - Fixed by no longer excluding `framework/src/neml2` from
    `moose_framework` when NEML2 is disabled.

- Verified the focused LSP completion chain that exposed the NEML2 action
  registration issue.

```bash
cd unit
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ../build-cmake-review/unit/moose_unit --gtest_filter='MooseServerTest.InitializeAndInitialized:MooseServerTest.DocumentOpenAndDiagnostics:MooseServerTest.DocumentOpenAndSymbols:MooseServerTest.DocumentChangeAndDiagnostics:MooseServerTest.DocumentChangeAndSymbols:MooseServerTest.CompletionMeshDefaultedType:MooseServerTest.CompletionDocumentRootLevel'
```

Result:

```text
7/7 tests passed
```

- Verified full `moose_unit` CTest passes.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH ctest --test-dir build-cmake-review -R '^moose_unit$' --output-on-failure
```

Result:

```text
1/1 tests passed
```

- Confirmed linker warnings do not block current verified targets.
  - Observed warnings include duplicate rpaths, deprecated `-ld64`, and
    compact-unwind warnings from MPI/PETSc/MUMPS libraries.

- Built representative module targets successfully before attempting all
  modules. Do not repeat this step unless a later change invalidates the build
  tree.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target misc -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target fluid_properties -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target heat_transfer -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target solid_mechanics -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target navier_stokes -j 8
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target thermal_hydraulics -j 8
```

Result:

```text
[100%] Built target misc
[100%] Built target fluid_properties
[100%] Built target heat_transfer
[100%] Built target solid_mechanics
[100%] Built target navier_stokes
[100%] Built target thermal_hydraulics
```

Produced or reused:

```text
build-cmake-review/modules/libmisc.dylib
build-cmake-review/modules/libfluid_properties.dylib
build-cmake-review/modules/libray_tracing.dylib
build-cmake-review/modules/libheat_transfer.dylib
build-cmake-review/modules/libsolid_mechanics.dylib
build-cmake-review/modules/librdg.dylib
build-cmake-review/modules/libnavier_stokes.dylib
build-cmake-review/modules/libsolid_properties.dylib
build-cmake-review/modules/libthermal_hydraulics.dylib
```

Notes:

- `heat_transfer` pulled in and built `ray_tracing`.
- `navier_stokes` pulled in and built `rdg`.
- `thermal_hydraulics` reused previous representative targets and additionally
  built `solid_properties`.
- The same linker warnings remain non-blocking: duplicate rpaths, deprecated
  `-ld64`, and compact-unwind warnings from MPI/PETSc/MUMPS libraries.

- Verified all existing `./build/modules/lib*.dylib` files on 2026-05-04.
  This check was against the user's `build/` tree, not `build-cmake-review/`.

```bash
file build/modules/lib*.dylib
otool -L build/modules/lib*.dylib
python3 -c "<isolated ctypes.CDLL() check for each build/modules/lib*.dylib>"
```

Result:

```text
PASS isolated dlopen build/modules/libchemical_reactions.dylib
PASS isolated dlopen build/modules/libfluid_properties.dylib
PASS isolated dlopen build/modules/libheat_transfer.dylib
PASS isolated dlopen build/modules/libmisc.dylib
PASS isolated dlopen build/modules/libnavier_stokes.dylib
PASS isolated dlopen build/modules/libray_tracing.dylib
PASS isolated dlopen build/modules/librdg.dylib
PASS isolated dlopen build/modules/libsolid_mechanics.dylib
PASS isolated dlopen build/modules/libsolid_properties.dylib
PASS isolated dlopen build/modules/libthermal_hydraulics.dylib
SUMMARY 10 dylibs loaded successfully in isolated processes
```

Notes:

- `file` reported all 10 files as `Mach-O 64-bit dynamically linked shared
  library arm64`.
- `otool -L` returned success for all 10 dylibs.
- A first same-process `ctypes.CDLL(..., RTLD_LOCAL | RTLD_NOW)` loop loaded
  the first 9 dylibs but failed on `libthermal_hydraulics.dylib` with a
  `RadiativeHeatFluxBCBaseTempl<false>::validParams()` lookup error. This was
  a test-mode artifact: isolated-process dlopen and direct default/global
  dlopen of `libthermal_hydraulics.dylib` both passed, and
  `libheat_transfer.dylib` exports the referenced symbol.
- This verifies dylib loadability only. Running an input file still requires a
  corresponding executable such as `build/modules/misc` or a built
  `<module>_exe` target.

- Built and smoke-tested module executables in the user's `build/` tree on
  2026-05-04. This is the executable-level check, not just dylib loadability.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build --target chemical_reactions_exe fluid_properties_exe heat_transfer_exe misc_exe navier_stokes_exe ray_tracing_exe rdg_exe solid_mechanics_exe solid_properties_exe thermal_hydraulics_exe -j 8
```

Result:

```text
Build completed successfully.
Produced 10 Mach-O 64-bit executable arm64 files in build/modules/.
```

Produced:

```text
build/modules/chemical_reactions
build/modules/fluid_properties
build/modules/heat_transfer
build/modules/misc
build/modules/navier_stokes
build/modules/ray_tracing
build/modules/rdg
build/modules/solid_mechanics
build/modules/solid_properties
build/modules/thermal_hydraulics
```

Verified file type:

```bash
file build/modules/chemical_reactions build/modules/fluid_properties build/modules/heat_transfer build/modules/misc build/modules/navier_stokes build/modules/ray_tracing build/modules/rdg build/modules/solid_mechanics build/modules/solid_properties build/modules/thermal_hydraulics
```

Result:

```text
All 10 files reported as Mach-O 64-bit executable arm64.
```

Smoke-tested executable startup/help with elevated runtime permissions:

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH
for exe in chemical_reactions fluid_properties heat_transfer misc navier_stokes ray_tracing rdg solid_mechanics solid_properties thermal_hydraulics; do
  build/modules/$exe --help >/dev/null 2>&1
  rc=$?
  printf '%s %s\n' "$exe" "$rc"
done
```

Result:

```text
chemical_reactions 0
fluid_properties 0
heat_transfer 0
misc 0
navier_stokes 0
ray_tracing 0
rdg 0
solid_mechanics 0
solid_properties 0
thermal_hydraulics 0
```

Notes:

- Running the same `--help` loop inside the sandbox returned 134 for every
  executable because PETSc/OpenMPI startup was denied by sandboxed
  `bind()` / `getdomainname()` calls. Re-running outside the sandbox passed.
- The build still emits non-blocking linker warnings: duplicate rpaths,
  deprecated `-ld64`, and compact-unwind warnings from MPI/PETSc/MUMPS
  libraries.

## TODO

- Decide whether `test/tests/outputs/format/out.n.1.0` should be restored or
  intentionally removed.

- Decide how to handle ignored `CMakeLists.txt` files.
  - Option A: stage them with `git add -f`.
  - Option B: add explicit `.gitignore` unignore rules for `CMakeLists.txt`.

- Keep `build-cmake-review/` uncommitted unless the user explicitly wants build
  artifacts tracked.

- Attempt full all-target build in `build-cmake-review/` after representative
  module targets and the user's `build/` module executables pass.

```bash
PATH=/Users/chenghau.yang/miniforge/envs/moose/bin:$PATH cmake --build build-cmake-review --target all -j 8
```

- Run `ctest -N` and inspect all registered tests for correct executable paths.

- Fix any remaining CTest registrations that use bare executable names instead
  of `$<TARGET_FILE:...>`.

- Run a narrow CTest set before broader coverage.
  Start with:
  - `moose_test_diffusion`
  - `moose_unit`
  - one or two low-dependency module unit tests

- Verify `python/TestHarness/TestHarness.py` against a real CMake-built test
  executable from a real test directory, not only through CTest.

- Compare CMake source inclusion against Make rules for:
  - framework optional feature directories
  - app test sources
  - module test sources
  - module unit tests
  - generated sources, if any

- Compare compile definitions and flags against Make for:
  - `METHOD`
  - `MOOSE_UNIT_TEST`
  - `WASP_ENABLED`
  - `ADFPARSER_INCLUDES`
  - forced include of `MooseConfig.h`

- Verify module dependency ordering against `modules/modules.mk`.

- Add or document feature flag behavior for optional integrations:
  - Kokkos
  - LibTorch
  - MFEM
  - NEML2

- Update `MOOSE_CMAKE_BUILD_GUIDE.md` so its phase/status wording matches the
  current state:
  - framework builds
  - `moose_test` builds
  - `moose_test_diffusion` passes
  - `moose_unit` builds and passes through CTest
  - representative module dylibs build
  - module executables in `build/` build and pass `--help` smoke tests

- Document the sandbox/runtime caveat for PETSc/OpenMPI if this workflow will
  be repeated inside Codex.

- Add guide examples for building:
  - framework only
  - `moose_test`
  - unit tests
  - selected modules

- After all TODO items are complete, state explicitly that full Make parity has
  been reached or list the remaining known differences.
