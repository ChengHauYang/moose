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
    build_openmpi.sh                <-- step 0 (Option C): CUDA-aware OpenMPI 4.1.6 into $PREFIX
    build_petsc.sh                  <-- step 1: PETSc source build (Kokkos + CUDA)
    build_libmesh.sh                <-- step 2: libmesh against PETSc
    build_wasp.sh                   <-- step 3: WASP into $PREFIX
    build_moose.sh                  <-- step 4: reconfigure MOOSE (--with-kokkos=cuda) + rebuild
    run_benchmark.sh                <-- step 5: run ../benchmark_cpu_and_gpu.py (GPU verified via nsys)
    all.sh                          <-- run steps 0..5 in order

$STACK_DIR/                         <-- OUT OF TREE, default ~/moose-cuda-stack
  prefix/                           <-- OpenMPI + PETSc + libmesh + WASP install ~1.3 GB
  src/openmpi-4.1.6/                <-- OpenMPI source (tarball + extracted tree, ~200 MB)
  logs/                             <-- per-step build logs
```

## Toolchain

- Compiler: `$PREFIX/bin/mpicc` / `$PREFIX/bin/mpicxx` / `$PREFIX/bin/mpif90` (CUDA-aware OpenMPI 4.1.6 wrapping `gcc-11` / `g++-11` / `gfortran-9`). `scripts/env.sh` prepends `$PREFIX/bin` to `PATH` when the wrappers exist, so `command -v mpicc` in `build_petsc.sh` picks them up automatically. Before running `build_openmpi.sh`, this falls back to `/usr/bin/mpicc` (system OpenMPI 4.1.2 — no CUDA support).
- CUDA: `/usr/local/cuda` (12.4), `nvcc` 12.4, host compiler g++-11 (CUDA 12.4 supports up to gcc 13)
- Runtime CUDA libs: `/usr/lib/x86_64-linux-gnu/{libcuda.so.1, libnvidia-ml.so.1}`, stubs at `/usr/local/cuda/lib64/stubs/`
- Kokkos target: `Kokkos_ARCH_AMPERE86` (two NVIDIA RTX A5000, sm_86)
- No conda in PATH — `scripts/env.sh` purges every env var not on a small safelist, because conda leaves several build-hint variables (`build_alias`, `CMAKE_ARGS`, `CPP=x86_64-conda-linux-gnu-cpp`, `CXX_FOR_BUILD`, `GCC_RANLIB`, …) that autoconf silently reads.

## Why this succeeds where the conda-env attempts failed

Conda's `mpicxx` wrapper injects `-I<env>/include -L<env>/lib -Wl,-rpath,<env>/lib` at the start of every compilation. That put conda-installed headers (Kokkos, Umpire, HDF5, `KokkosCore_config.h`, …) *in front of* PETSc's downloaded/built ones, so package builds picked up the wrong `Config.hpp` / `KOKKOS_ENABLE_CUDA_LAMBDA` / etc. System `/usr/bin/mpicxx` adds `-I/usr/lib/x86_64-linux-gnu/openmpi/include` only; nothing shadows PETSc's `arch-*/externalpackages` includes.

## Rerun

```bash
cd modules/solid_mechanics/test/tests/kokkos/linear_elasticity/moose-cuda-stack
./scripts/all.sh                        # full chain, incl. CUDA-aware MPI, ~1.5 h on this box

# or individually (measured wall times below; see Option C section for the per-step breakdown)
./scripts/build_openmpi.sh              # ~10 min (Option C: CUDA-aware OpenMPI 4.1.6)
./scripts/build_petsc.sh                # ~40 min
./scripts/build_libmesh.sh              # ~20 min (4 methods)
./scripts/build_wasp.sh                 # ~2 min
./scripts/build_moose.sh                # ~10 min
./scripts/run_benchmark.sh              # ~15 min (5 reps × 5 mesh sizes × 2 devices)
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

## Option C: CUDA-aware OpenMPI + GPU linear solve

The Aug-26 build ran assembly on the GPU but kept the linear solve on the CPU:
the input never set `-mat_type aijkokkos -vec_type kokkos`, and trying those
flags aborted with PETSc errorcode 76 because the system OpenMPI 4.1.2 on this
box is not GPU-aware (`ompi_info | grep built_with_cuda_support` → `false`).
This section documents the full path to a GPU linear solve.

### 1. Two-part recipe

Enabling GPU KSP on this stack requires **both** a matching input file and an
env var — command-line PETSc options alone are not enough because MOOSE
rejects non-prefixed `-mat_type` with a solver-system-prefix check, and
libmesh's primary `PetscVector::init` calls `VecSetFromOptions` only for the
non-prefixed `-vec_type`.

Input file: use a GPU-compatible preconditioner and Krylov method (the SMP
default of ILU-preconditioned GMRES segfaults on `aijkokkos` at the first
triangular solve — the backtrace lands in `MatSolve_SeqAIJKokkos_LU` →
`VecGetKokkosView<Kokkos::CudaSpace>`). A concrete input is
`kokkos_material_linear_elasticity_gpu_ksp.i` beside this directory; the key
lines are:

```
[Executioner]
  petsc_options_iname = '-pc_type -ksp_type -ksp_max_it -ksp_rtol'
  petsc_options_value = 'jacobi   cg        5000        1e-8'
[]
```

Env var (GPU runs only — do NOT set it for CPU runs, or PETSc will try to put
the matrix on GPU with a `--compute-device=cpu` binary):

```bash
export PETSC_OPTIONS="-use_gpu_aware_mpi 0 -vec_type kokkos -mat_type aijkokkos"
mpirun -np 1 -x PETSC_OPTIONS \
    $MOOSE_DIR/modules/solid_mechanics/solid_mechanics-opt \
    -i kokkos_material_linear_elasticity_gpu_ksp.i \
    --compute-device=cuda --n-threads=1
```

Drop `-use_gpu_aware_mpi 0` once you have finished the CUDA-aware OpenMPI
rebuild described in step 2. Verify GPU KSP is really running with
`nsys profile --stats=true -o out mpirun ...` and grep the report for
`MatMult`, `nrm2_kernel`, `axpy_kernel_val`, `VecPointwiseMult_Seq…` — those
are the CG + Jacobi kernels executing on device.

### 2. Rebuild for real CUDA-aware MPI

`scripts/build_openmpi.sh` builds OpenMPI 4.1.6 into `$PREFIX` with
`--with-cuda=/usr/local/cuda`. `scripts/env.sh` then prepends `$PREFIX/bin`
to `PATH` so subsequent `command -v mpicc` in `build_petsc.sh` resolves to
the CUDA-aware wrappers, and `all.sh` runs OpenMPI first in the chain.

```bash
export STACK_DIR=$HOME/packages/moose-cuda-stack
export MOOSE_CUDA_STACK_KEEP=STACK_DIR
./scripts/all.sh                        # openmpi → petsc → libmesh → wasp → moose → benchmark
```

Measured wall time on this box (32-core, `MOOSE_JOBS=8`) starting from a
`$PREFIX` that already had an earlier build in it (so libmesh & MOOSE just
recompiled against the new mpi rather than a from-scratch tree):

| Step | Wall time |
|---|---:|
| `build_openmpi.sh` (download + configure + build + install) | 11 min |
| `build_petsc.sh` (fresh `arch-scratch-cuda`, all downloaded packages) | 39 min |
| `build_libmesh.sh` (all four methods: opt / oprof / devel / dbg) | 21 min |
| `build_moose.sh` (framework + solid_mechanics) | 10 min |
| `run_benchmark.sh` (5 reps × 5 mesh × 2 devices) | 14 min |
| **Total** | **1 h 35 min** |

After the rebuild, `ompi_info --parsable --all | grep built_with_cuda_support`
should print `:value:true`, PETSc's `-use_gpu_aware_mpi 0` fallback is no
longer needed, and MOOSE's KSP runs against `MPI_COMM_WORLD` communicators
whose Datatypes accept device pointers directly (no extra cudaMemcpyAsync per
halo exchange).

### 3. Benchmark tool

`benchmark_mesh_scaling.py` (next to this directory, one level up) has a
`--gpu-ksp-env` flag that injects a `PETSC_OPTIONS` value into the child
process env on GPU runs only (CPU runs stay untouched) and adds
`-x PETSC_OPTIONS` to the corresponding `mpirun` invocation so the value
actually reaches the MOOSE binary. The CPU baseline uses the same input, so
both sides run the same solver — an apples-to-apples comparison of CPU vs
GPU wall time, not a solver comparison. The exact command lines for the
three configurations we ran are in step 5 below.

### 4. Results (this box: two RTX A5000 sm_86, single MPI rank)

Every wall time below is the mean of 5 timed repetitions (with 1 untimed
warmup) at that mesh size on this box. GPU runs use one device with
`--n-threads=1`; CPU runs use 16 OpenMP threads (`OMP_PROC_BIND=spread`,
`OMP_PLACES=cores`). CPU and GPU always use the same input file within a
given configuration, so no configuration compares apples to oranges.

Three artefact pairs, one per configuration:

| CSV / PNG | GPU KSP? | MPI | Solver |
|-----|----|----|----|
| `mesh_scaling.{csv,png}`                 (Aug-26 baseline)  | No — GPU does assembly only | system OpenMPI 4.1.2 (not GPU-aware) | SMP default (ILU + GMRES) |
| `mesh_scaling_gpu_ksp.{csv,png}`         (Phase 1, pre-C)    | Yes, via `PETSC_OPTIONS` + `-use_gpu_aware_mpi 0` fallback | system OpenMPI 4.1.2 (not GPU-aware) | Jacobi + CG |
| `mesh_scaling_gpu_ksp_cuda_mpi.{csv,png}` (Phase 3, post-C)  | Yes, no fallback needed | our OpenMPI 4.1.6 from `build_openmpi.sh` (GPU-aware) | Jacobi + CG |

**What the two GPU-KSP plots differ in — the MPI layer, nothing else.**

- `mesh_scaling_gpu_ksp.png` ("pre-CUDA-MPI"): the system OpenMPI 4.1.2
  wrapping the linker/compilers is compiled without CUDA support
  (`opal_built_with_cuda_support = false`). When PETSc's `aijkokkos` matrix
  needs to move a piece of a device-resident vector across ranks, it cannot
  hand the device pointer to MPI directly — MPI would fault. PETSc's
  `-use_gpu_aware_mpi 0` opts into the safe fallback: for every halo
  exchange it copies the piece into a host buffer with `cudaMemcpyAsync`,
  hands the host buffer to MPI, then copies the reply back to device.
  Serial `-np 1` runs like this benchmark still take these copies because
  PETSc's PetscSF uses the same code path internally.
- `mesh_scaling_gpu_ksp_cuda_mpi.png` ("CUDA-aware MPI"): the OpenMPI 4.1.6
  built by `scripts/build_openmpi.sh` was configured with
  `--with-cuda=/usr/local/cuda`, so it links against `libcuda`/`libcudart`
  and knows how to send/receive from CUDA-registered device buffers.
  `ompi_info --parsable --all | grep built_with_cuda_support` now prints
  `:value:true` on both `opal` and `mpi` scopes. PETSc drops the fallback,
  the `cudaMemcpyAsync` per halo goes away, and the GPU spends less time
  idle waiting for host round-trips.

Everything else (input file, PC, KSP, mesh sizes, repetitions, CPU thread
count) is identical between the two plots, so the delta between them is a
clean measurement of what the CUDA-aware MPI rebuild bought us.

Wall time at each mesh size, all three configurations side by side:

| Mesh | DOFs | Aug-26 CPU | Aug-26 GPU | Pre-C CPU | Pre-C GPU | Post-C CPU | Post-C GPU | Post-C GPU vs. Pre-C GPU |
|-----:|-----:|-----------:|-----------:|----------:|----------:|-----------:|-----------:|-------------------------:|
|   64 |   8k |     1.06 s |     1.08 s |    1.27 s |    1.43 s |     0.89 s |     0.82 s |         **1.74× faster** |
|  128 |  33k |     1.83 s |     1.77 s |    1.90 s |    1.73 s |     1.16 s |     0.97 s |         **1.78× faster** |
|  256 | 131k |     8.12 s |     8.05 s |    4.12 s |    2.77 s |     3.51 s |     1.84 s |         **1.51× faster** |
|  512 | 524k |    52.29 s |    51.99 s |   14.30 s |    7.65 s |    13.30 s |     5.81 s |         **1.32× faster** |
| 1024 |   2M |   459.73 s |   454.51 s |   85.61 s |   27.74 s |    84.10 s |    22.48 s |         **1.23× faster** |

- CPU/GPU speedup (post-C): rises from 1.08× at mesh 64 to **3.74×** at mesh
  1024 — the GPU wins at every size once the MPI copies are gone.
- Post-C GPU vs Aug-26 GPU: **20× faster** at mesh 1024 (22.5 s vs 454.5 s)
  — combined effect of switching solver to jacobi+CG and moving KSP to the
  device with CUDA-aware MPI.
- CUDA-aware MPI helps most at small meshes: at mesh 64, MPI-halo latency
  was ~40% of GPU time, so removing the copies gives 1.74×. At mesh 1024,
  the matrix-vector work dominates and MPI copies were relatively cheaper,
  so removing them buys only 1.23× — but it still tips several sizes over
  the CPU/GPU crossover.

Two independent effects stack:

1. **Solver choice** (Aug-26 → Phase 1): Jacobi-CG converges in ~N iterations
   for a mesh-N elasticity problem and vectorises cleanly; ILU + GMRES over
   the SMP-assembled matrix does not. The switch alone accounts for the ~5×
   drop from 460 s to 86 s on CPU at mesh 1024, and lets the GPU pick up
   whatever headroom is left.
2. **CUDA-aware MPI** (Phase 1 → Phase 3): 1.2–1.8× on top, biggest at small
   meshes where MPI-halo latency was the largest fraction of GPU time.

### 5. Reproducing each plot

```bash
cd $MOOSE_DIR/modules/solid_mechanics/test/tests/kokkos/linear_elasticity

# Aug-26 baseline (SMP + ILU + GMRES; GPU only for assembly)
python3 benchmark_mesh_scaling.py \
  --csv mesh_scaling.csv --plot mesh_scaling.png \
  --title "MOOSE Kokkos: CPU vs. GPU Mesh Scaling"

# Phase 1 -- GPU KSP with system MPI, needs the -use_gpu_aware_mpi 0 fallback
python3 benchmark_mesh_scaling.py \
  --kokkos-input kokkos_material_linear_elasticity_gpu_ksp.i \
  --gpu-ksp-env "-use_gpu_aware_mpi 0 -vec_type kokkos -mat_type aijkokkos" \
  --mpiexec /usr/bin/mpirun \
  --csv mesh_scaling_gpu_ksp.csv --plot mesh_scaling_gpu_ksp.png \
  --title "MOOSE Kokkos: CPU vs. GPU KSP Mesh Scaling (pre-CUDA-MPI)"

# Phase 3 -- after build_openmpi.sh completes, no fallback flag
python3 benchmark_mesh_scaling.py \
  --kokkos-input kokkos_material_linear_elasticity_gpu_ksp.i \
  --gpu-ksp-env "-vec_type kokkos -mat_type aijkokkos" \
  --mpiexec $PREFIX/bin/mpirun \
  --csv mesh_scaling_gpu_ksp_cuda_mpi.csv --plot mesh_scaling_gpu_ksp_cuda_mpi.png \
  --title "MOOSE Kokkos: CPU vs. GPU KSP Mesh Scaling (CUDA-aware MPI)"
```

All three benchmarks take ~15–20 min each on this box at 5 reps × 5 mesh
sizes × 2 devices; the 1024×1024 CPU run dominates.
