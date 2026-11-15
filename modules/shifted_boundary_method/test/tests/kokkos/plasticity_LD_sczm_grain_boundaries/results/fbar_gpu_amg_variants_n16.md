# GPU BoomerAMG variants, plasticity_LD_sczm_grain_boundaries, F-bar ON, n=16

PMIS coarsening with l1-Jacobi relaxation remains the tuned GPU baseline. Symmetric-SOR/Jacobi
relaxation, the CPU default, cuts KSP iterations 3-4x but makes each GPU iteration about 6x more
expensive, so KSPSolve and wall time both rise. No GPU variant beats step 4 (CPU PETSc).

Figure and data: `fbar_gpu_amg_variants_n16.png/.csv`. Raw runs, scripts and verification output:
`../raw/fbar_gpu_amg_variants_n16/`.

## Setup

- n=16 (384 elements, 968 DOFs), F-bar ON, 20 timesteps to t = 5.0, steps 5 (AIJKOKKOS / kokkos vec)
  and 6 (MATHYPRE / cuda vec), 3 timed runs plus one Nsight Systems profile per step, GPU 0 only.
- Every variant keeps the tuned options (`AMG=tuned`: `max_iter 1`, `tol 0`, `interp_type ext+i`,
  `agg_nl 1`, `strong_threshold 0.6`) and adds through `EXTRA_PETSC_ARGS`:

  | Variant | Added options | Effective coarsening / relaxation (-ksp_view) |
  |---|---|---|
  | tuned baseline | none | PMIS / l1scaled-Jacobi |
  | PMIS + sym-SOR/Jacobi | `-pc_hypre_boomeramg_relax_type_all symmetric-SOR/Jacobi` | PMIS / symmetric-SOR/Jacobi |
  | HMIS + sym-SOR/Jacobi | the above plus `-pc_hypre_boomeramg_coarsen_type HMIS` | HMIS / symmetric-SOR/Jacobi |
  | HMIS + l1-Jacobi | `-pc_hypre_boomeramg_coarsen_type HMIS` | HMIS / l1scaled-Jacobi |

- Steps 1-4 are unaffected by these options; the step 4 reference and the baseline rows come from
  `../raw/fbar_tuned_amg_n16/`.

## Results

Mean of 3 runs; speedup is step 4 wall time divided by the variant's wall time.

| Configuration | Step | Wall time (s), runs | Mean +- std (s) | Speedup vs step 4 | KSP iterations | KSPSolve (s) | ms per KSP iteration |
|---|---|---|---|---|---|---|---|
| CPU PETSc reference | 4 | 59.07, 59.09, 62.20 | 60.12 +- 1.80 | 1.00 | 5335 | 1.80 | 0.34 |
| tuned baseline (PMIS + l1-Jacobi) | 5 | 73.67, 69.33, 69.01 | 70.67 +- 2.60 | 0.85 | 22818 | 11.46 | 0.50 |
| | 6 | 72.00, 69.17, 69.15 | 70.11 +- 1.64 | 0.86 | 22818 | 11.12 | 0.49 |
| PMIS + sym-SOR/Jacobi | 5 | 77.80, 78.32, 77.75 | 77.96 +- 0.32 | 0.77 | 6535 | 19.91 | 3.05 |
| | 6 | 86.39, 82.66, 82.91 | 83.99 +- 2.09 | 0.72 | 7779 | 23.75 | 3.05 |
| HMIS + sym-SOR/Jacobi | 5 | 76.85, 77.58, 76.70 | 77.04 +- 0.47 | 0.78 | 5830 | 17.98 | 3.08 |
| | 6 | 80.47, 79.21, 79.86 | 79.85 +- 0.63 | 0.75 | 6961 | 21.51 | 3.09 |
| HMIS + l1-Jacobi | 5 | 71.99, 68.57, 71.56 | 70.71 +- 1.86 | 0.85 | 19646 | 10.30 | 0.52 |
| | 6 | 74.53, 68.87, 69.18 | 70.86 +- 3.18 | 0.85 | 19646 | 10.12 | 0.52 |

KSP iterations are totals over the 100 Newton solves (20 timesteps x 5 Newton iterations).

## Solver trade-offs

- Symmetric-SOR/Jacobi: on the device hypre applies Gauss-Seidel through sparse triangular solves
  (`hypre_BoomerAMGRelaxHybridGaussSeidelDevice`), which have limited parallelism; a KSP iteration
  costs about 3.1 ms against 0.5 ms with l1-Jacobi. The iteration count falls to near the CPU count
  (HMIS + sym-SOR/Jacobi, step 5: 5830 vs 5335) but KSPSolve rises 57-114% and wall time by 6-14 s.
- With symmetric-SOR/Jacobi, step 6 (MATHYPRE) needs about 19% more iterations than step 5
  (AIJKOKKOS); with l1-Jacobi the two are identical. The cause was not investigated.
- HMIS instead of PMIS: 14% fewer iterations and about 10% less KSPSolve time with l1-Jacobi, but the
  wall-time change is within the run-to-run spread.
- At 968 DOFs even the cheapest GPU KSPSolve (10.1 s) is about 5.5x the CPU's 1.8 s, and that
  difference accounts for the gap between steps 5-6 and step 4. AMG options do not close it at this
  problem size.

## Options that do not run on the GPU

One-timestep smoke tests (`../raw/fbar_gpu_amg_variants_n16/smoke/`) with hypre 3.1:

- Falgout coarsening (`-pc_hypre_boomeramg_coarsen_type Falgout`, the CPU default): terminated by
  signal 11 during preconditioner setup. hypre calls its host Ruge-Stueben coarsening on device data.
- CF-ordered relaxation (`-pc_hypre_boomeramg_no_CF false` with symmetric-SOR/Jacobi): terminated by
  signal 11 in the first preconditioner application. CF relaxation forces hypre's host relaxation path
  (the device path is marked "TODO implement CF relax on GPUs").

The closest device-capable match to the CPU preconditioner is therefore HMIS + symmetric-SOR/Jacobi
without CF ordering.

## Verification and data notes

- All 18 runs reach t = 5.0 in 20 timesteps with 100 Newton iterations and no failed timesteps. The
  postprocessor CSV histories agree with tuned step 1 rep 1 to a maximum relative difference of
  7.9e-12, and the exodus disp_x / disp_y fields to 2.4e-12
  (`../raw/fbar_gpu_amg_variants_n16/verify_gpu_amg_variants_n16.out`).
- Peak GPU memory is unchanged from the tuned baseline: 884-892 MiB for step 5 and 4722-4730 MiB for
  step 6.
- The Nsight profiles of PMIS + sym-SOR/Jacobi did not finish importing (only `.qdstrm` files), so its
  `cuda_kernel_time_s` and H2D/D2H columns are N/A. Wall times and KSP data come from the timed runs and
  are unaffected.
