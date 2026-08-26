# From-scratch MOOSE-Kokkos-CUDA stack

Goal: enable the GPU leg of `../benchmark_cpu_and_gpu.py` by rebuilding PETSc + libmesh + WASP + MOOSE from source against **system compilers** (no conda) so we sidestep conda's `-I<env>/include` header-shadowing problem that broke every conda-env attempt. See `ATTEMPTS.md` for the full attempt-by-attempt journey; `SUMMARY.md` for the one-page result.

## Layout

This directory (in-tree, under `modules/solid_mechanics/test/tests/kokkos/linear_elasticity/moose-cuda-stack/`) contains only the *source* — scripts and documentation. Build artefacts land in a separate `$STACK_DIR` (default `~/moose-cuda-stack`) so the repo stays clean.

```
moose-cuda-stack/                   <-- this dir (in repo)
  README.md                         <-- this file
  ATTEMPTS.md                       <-- chronological log of everything tried
  SUMMARY.md                        <-- one-page summary of the final state
  scripts/
    env.sh                          <-- source this to get the clean toolchain env
    build_petsc.sh                  <-- step 1: PETSc source build (Kokkos + CUDA)
    build_libmesh.sh                <-- step 2: libmesh against PETSc
    build_wasp.sh                   <-- step 3: WASP into $PREFIX
    build_moose.sh                  <-- step 4: reconfigure MOOSE (--with-kokkos=cuda) + rebuild
    run_benchmark.sh                <-- step 5: run ../benchmark_cpu_and_gpu.py (GPU verified via nsys)
    all.sh                          <-- run steps 1..5 in order

$STACK_DIR/                         <-- OUT OF TREE, default ~/moose-cuda-stack
  prefix/                           <-- PETSc + libmesh + WASP install ~1.1 GB
  logs/                             <-- per-step build logs
```

## Toolchain

- Compiler: `/usr/bin/mpicc` / `/usr/bin/mpicxx` / `/usr/bin/mpif90` (system OpenMPI 4.x wrapping `gcc-11` / `g++-11` / `gfortran-9`)
- CUDA: `/usr/local/cuda` (12.4), `nvcc` 12.4, host compiler g++-11 (CUDA 12.4 supports up to gcc 13)
- Runtime CUDA libs: `/usr/lib/x86_64-linux-gnu/{libcuda.so.1, libnvidia-ml.so.1}`, stubs at `/usr/local/cuda/lib64/stubs/`
- Kokkos target: `Kokkos_ARCH_AMPERE86` (two NVIDIA RTX A5000, sm_86)
- No conda in PATH — `scripts/env.sh` purges every env var not on a small safelist, because conda leaves several build-hint variables (`build_alias`, `CMAKE_ARGS`, `CPP=x86_64-conda-linux-gnu-cpp`, `CXX_FOR_BUILD`, `GCC_RANLIB`, …) that autoconf silently reads.

## Why this succeeds where the conda-env attempts failed

Conda's `mpicxx` wrapper injects `-I<env>/include -L<env>/lib -Wl,-rpath,<env>/lib` at the start of every compilation. That put conda-installed headers (Kokkos, Umpire, HDF5, `KokkosCore_config.h`, …) *in front of* PETSc's downloaded/built ones, so package builds picked up the wrong `Config.hpp` / `KOKKOS_ENABLE_CUDA_LAMBDA` / etc. System `/usr/bin/mpicxx` adds `-I/usr/lib/x86_64-linux-gnu/openmpi/include` only; nothing shadows PETSc's `arch-*/externalpackages` includes.

## Rerun

```bash
cd modules/solid_mechanics/test/tests/kokkos/linear_elasticity/moose-cuda-stack
./scripts/all.sh                        # full chain (~2 h)

# or individually
./scripts/build_petsc.sh                # ~45 min
./scripts/build_libmesh.sh              # ~90 min (4 methods)
./scripts/build_wasp.sh                 # ~2 min
./scripts/build_moose.sh                # ~15 min
./scripts/run_benchmark.sh              # ~15 min (mesh 256, 5 reps)
```

Override the out-of-tree install prefix:

```bash
export STACK_DIR=$HOME/somewhere-else       # default: ~/moose-cuda-stack
export MOOSE_CUDA_STACK_KEEP="STACK_DIR"    # keep STACK_DIR through env.sh's purge
./scripts/all.sh
```

To USE the CUDA MOOSE from a fresh shell later:

```bash
. modules/solid_mechanics/test/tests/kokkos/linear_elasticity/moose-cuda-stack/scripts/env.sh
export PETSC_DIR=$PREFIX PETSC_ARCH="" LIBMESH_DIR=$PREFIX WASP_DIR=$PREFIX
$MOOSE_DIR/modules/solid_mechanics/solid_mechanics-opt -i <input.i> --compute-device=cuda
```

To go back to the conda `moose` env's CPU stack: open a fresh shell (don't source `env.sh`), `conda activate moose`, then `cd framework && make clean && make -j` to rebuild against conda's PETSc/libmesh.
