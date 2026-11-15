# plasticity_LDinSCZM benchmark figures

Every figure is `<name>.png` with its per-step data in `<name>.csv`, made by `../analyze_results.py`
from the raw runs (logs, `.time`, exodus, CSV, Nsight profiles, `run.log`, `gpu0_samples.csv`) in
`../raw/<name>/`. Names follow `../../plasticity_LD_sczm_grain_boundaries/results/README.md`:
`fbar_<AMG configuration>_amg_n<N>`. The bulk and the TL-SCZM interface always use total-mode F-bar here.

| Figure | Content |
|---|---|
| `fbar_tuned_amg_n16` | tuned AMG, N=16, six steps, mean of 3 runs |

Reproduce with

    RESULTS_DIR=$PWD/raw/fbar_tuned_amg_n16 MESH_N=16 AMG=tuned ./run_benchmarks.sh
    python3 analyze_results.py raw/fbar_tuned_amg_n16 --output results/fbar_tuned_amg_n16.png \
      --title "F-bar ON, tuned AMG, N=16"

## fbar_tuned_amg_n16

- Mesh: N=16, 32 x 16 x 16 = 8192 HEX8 elements split into two blocks by the SCZM interface, 29478 DOFs.
- Loading: the script defaults, DT = 5e-3 and NUM_STEPS = 5 (t = 0.025).
- Hardware: GPU 0 (RTX A5000), runs sequential.
- Preconditioner (`AMG=tuned`): `gmres` + `hypre boomeramg` with `max_iter 1, tol 0, interp ext+i,
  agg_nl 1, strong_threshold 0.6`. These were the fastest options in the grain-boundary study.
  PCHYPRE still applies its separate host and device defaults:
  - steps 1-4: Falgout, symmetric-SOR/Jacobi, CF relaxation;
  - steps 5-6: PMIS, l1scaled-Jacobi, no CF relaxation.

| Step | Wall time (s), runs | Mean +- std (s) | Speedup vs step 1 / step 4 | NEML2 (s) | KSP its | KSPSolve (s) | Peak GPU memory (MiB) |
|---|---|---|---|---|---|---|---|
| 1 host / CPU NEML2 | 355.18, 357.07, 367.03 | 359.76 +- 6.37 | 1.00 / 0.12 | 148.2 | 220 | 4.74 | 214 |
| 2 host / GPU NEML2 | 216.88, 221.94, 216.87 | 218.56 +- 2.92 | 1.65 / 0.20 | 7.6 | 220 | 5.55 | 4524 |
| 3 Kokkos / CPU NEML2 | 190.81, 192.29, 189.67 | 190.92 +- 1.31 | 1.88 / 0.23 | 151.4 | 220 | 13.49 | 1486 |
| 4 Kokkos / GPU NEML2 | 45.17, 44.12, 44.88 | 44.72 +- 0.54 | 8.04 / 1.00 | 7.0 | 220 | 13.51 | 5652 |
| 5 + AIJKOKKOS | 33.20, 33.44, 33.54 | 33.39 +- 0.17 | 10.77 / 1.34 | 7.0 | 574 | 0.80 | 8108 |
| 6 + MATHYPRE | 28.86, 28.48, 28.19 | 28.51 +- 0.34 | 12.62 / 1.57 | 7.0 | 574 | 0.76 | 9936 |

Notes on the table:

- KSP iterations are totals over the 10 Newton solves (5 timesteps x 2 Newton iterations).
- Peak GPU memory comes from nvidia-smi samples taken every 0.5 s and matched to runs with the
  `run.log` timestamps. The idle baseline is 6 MiB.

## Findings

- **GPU PETSc beats CPU PETSc at this size, unlike the 2D grain-boundary case.**
  - Step 5 takes 11.3 s less than step 4, and step 6 takes 16.2 s less.
  - KSPSolve drops from 13.5 s to 0.8 s even though the GPU AMG needs 2.6x as many iterations
    (574 vs 220).
- **Step 6 vs step 5:** step 6 is 4.9 s faster. Step 5 converts its matrix to HYPRE format
  (MatConvert) once per PC setup, 10 times per run; step 6 assembles directly in HYPRE format and never
  converts. MatConvert does not appear in step 6's -log_view, so its bar is empty.
- **NEML2 dominates the CPU-NEML2 steps.** It takes about 150 s on the CPU and 7 s on the GPU (21x).
  This accounts for most of the step 1 -> 2 and step 3 -> 4 gains.
- **Kokkos assembly on the CPU PETSc steps:**
  - Kokkos assembly keeps the full allocated sparsity pattern: 6,180,174 nonzeros, against 2,290,554 in
    the compacted host matrix.
  - Iteration counts are unchanged, but CPU KSPSolve is 2.8x slower in steps 3-4 (13.5 s) than in
    steps 1-2 (4.7-5.5 s).
  - Steps 4-6 share the same pattern, so the step 4 vs 5 vs 6 comparison is not affected.
- **Correctness:**
  - Every run reaches t = 0.025 in 5 timesteps with no failed timesteps.
  - Postprocessor histories agree with step 1 rep 1 to a maximum relative difference of 3.5e-10
    (CSV column `max_rel_diff_vs_step1`).

## Data gaps

- The Nsight profiles of steps 1 and 2 did not import, so those GPU columns are N/A.
  - Only `.qdstrm` files exist.
  - Step 1 exceeded Nsight's 2 GB protobuf limit (a 9.1 GB string table; see
    `step1_ld_sczm_host_cpu_profile.log`). Step 2's importer failed without naming a cause (see
    `step2_ld_sczm_host_cuda_neml2_profile.log`).
  - Wall times and solver data come from the timed runs and are unaffected.
