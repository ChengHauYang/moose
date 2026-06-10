Please keep this even we totally get rid of MAKE

# How To Compile MOOSE With `make`

This repository uses the existing MOOSE Make-based build system as the source of truth.

This is a short developer guide for people and LLMs who need the normal compile flow without
reading the full installation docs first.

## What to know first

- MOOSE is not built from a top-level `make` command in the repository root.
- The common entry point for building the framework is the [`test/`](./test) directory.
- Dependencies such as PETSc, libMesh, and WASP are usually built first with scripts under
  [`scripts/`](./scripts).

## Normal build flow

### 1. Build dependencies

From the repository root:

```bash
export MOOSE_JOBS=6
export METHODS=opt

cd scripts
./update_and_rebuild_petsc.sh
./update_and_rebuild_libmesh.sh
./update_and_rebuild_wasp.sh
```

Notes:

- `MOOSE_JOBS` controls parallelism used by many MOOSE scripts.
- `METHODS=opt` tells dependency builds to build the optimized method only.
- If your environment was prepared another way, this step may already be done.

### 2. Build MOOSE

The documented build entry point is `test/`:

```bash
cd ../test
make -j 6
```

This builds the framework and the `moose_test` application using the repository's Make logic.

### 3. Run tests

Still from `test/`:

```bash
./run_tests -j 4
```

If the build is healthy, most tests should pass. Some tests may be skipped depending on the local
environment and optional packages.

## Other common Make roots

This repository is a multi-root build tree. People also run `make` in places such as:

- [`framework/`](./framework): core framework build root
- [`test/`](./test): main framework build and test root
- [`unit/`](./unit): framework unit tests
- [`modules/<module>/`](./modules): individual module roots
- [`stork/`](./stork): generated app scaffold

Use the Makefile in the subtree you are working on. Do not assume one top-level build command
covers the whole repository.

## Important practical rule

If you are unsure where to build:

1. Build dependencies from [`scripts/`](./scripts) if needed.
2. Build MOOSE from [`test/`](./test).
3. Build a specific module or app from its own directory only when you are working there.

## Source docs

This file is intentionally short. The detailed installation docs are here:

- [`modules/doc/content/getting_started/installation/index.md`](./modules/doc/content/getting_started/installation/index.md)
- [`modules/doc/content/getting_started/installation/build_petsc_and_libmesh.md.template`](./modules/doc/content/getting_started/installation/build_petsc_and_libmesh.md.template)
- [`modules/doc/content/getting_started/installation/build_moose.md.template`](./modules/doc/content/getting_started/installation/build_moose.md.template)
- [`modules/doc/content/getting_started/installation/test_moose.md`](./modules/doc/content/getting_started/installation/test_moose.md)
