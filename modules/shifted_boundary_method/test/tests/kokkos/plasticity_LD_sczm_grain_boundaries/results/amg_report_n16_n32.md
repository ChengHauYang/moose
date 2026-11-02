# Tuned vs simple BoomerAMG: plasticity_LD_sczm_grain_boundaries, F-bar ON, n=16 and n=32

Figures and per-step data:

| File | AMG | Mesh |
|---|---|---|
| `fbar_tuned_amg_n16.png/.csv` | tuned (new, raw runs in `../raw/fbar_tuned_amg_n16/`) | n=16: 384 elements, 968 DOFs |
| `fbar_tuned_amg_n32.png/.csv` | tuned (new, raw runs in `../raw/fbar_tuned_amg_n32/`) | n=32: 1536 elements, 3462 DOFs |
| `fbar_simple_amg_n16.png/.csv` | simple (baseline, raw runs in `../raw/fbar_simple_amg_n16/`) | n=16 |
| `fbar_simple_amg_n32.png/.csv` | simple (baseline, raw runs in `../raw/fbar_simple_amg_n32/`) | n=32 |

All four sets are F-bar ON, six steps, REPEATS=3 (all 3 used, none failed), one Nsight Systems profile
per step, GPU 0 only (RTX A5000), runs sequential. Raw tuned outputs: `../raw/fbar_tuned_amg_n{16,32}/`.

Tuned = the step inputs' `gmres` + `hypre boomeramg` plus
`-pc_hypre_boomeramg_max_iter 1 -pc_hypre_boomeramg_tol 0.0 -pc_hypre_boomeramg_interp_type ext+i
-pc_hypre_boomeramg_agg_nl 1 -pc_hypre_boomeramg_strong_threshold 0.6`, added identically to every step
(`AMG=tuned` in `run_benchmarks.sh`). Each step keeps its own mat/vec types: steps 1-4 `aij`/standard,
step 5 `aijkokkos`/kokkos, step 6 `hypre`/cuda. `automatic_scaling` and
`residual_and_jacobian_together` come from the step inputs.

## Effective AMG (from -ksp_view of every run; CSV columns amg_*)

The same options do not give the same preconditioner. PCHYPRE picks different defaults when the
matrix lives on the device (`petsc/src/ksp/pc/impls/hypre/impl/ihypre.c:315-347`), and the tuned
options do not override those defaults.

| | Steps 1-4 (CPU, seqaij) | Steps 5-6 (GPU, seqaijkokkos / hypre) |
|---|---|---|
| Coarsening | Falgout | PMIS |
| Relaxation | SOR/Jacobi (hypre relax 6) | l1scaled-Jacobi (hypre relax 18) |
| CF-relaxation | on | off |
| Interpolation | ext+i (simple: classical) | ext+i (simple: ext+i) |
| Strong threshold | 0.6 (simple: 0.25) | 0.6 (simple: 0.25) |
| Aggressive-coarsening levels | 1 (simple: 0) | 1 (simple: 0) |
| SpGEMM | hypre | cusparse |

Not printed by -ksp_view but also device-dependent: agg_interptype (CPU 4, GPU 7), keeptranspose and mod_rap2.

## Correctness

- Every run (36 tuned, 36 simple):
  - reaches t = 5.0 in 20 timesteps;
  - converges in 100 Newton iterations (5 per timestep);
  - has no failed timesteps or cutbacks.
- Full-history agreement with step1 rep1 (all 21 output times; script `/tmp/user/333475/verify_compare.py`,
  output in `/tmp/user/333475/analyzer_check/verify_*.out`):

  | Mesh | Postprocessor CSV, max relative difference | Exodus disp_x / disp_y, max field-relative difference |
  |---|---|---|
  | n=16 | 1.1e-11 | 6e-13 / 2.3e-12 |
  | n=32 | 5.2e-12 | 3e-13 / 1.4e-12 |

  These differences are solver-tolerance noise.
- Tuned and simple step1 agree to 1e-11 (CSV) and 2.7e-12 (exodus), so changing the AMG does not change the solution.

## Wall time (s): three raw runs, mean +- std

| n | Step | Tuned raw | Tuned mean +- std | Simple raw | Simple mean +- std |
|---|---|---|---|---|---|
| 16 | 1 | 65.81, 65.83, 65.51 | 65.72 +- 0.18 | 67.35, 66.83, 66.88 | 67.02 +- 0.29 |
| 16 | 2 | 67.91, 67.36, 66.75 | 67.34 +- 0.58 | 64.68, 65.01, 64.88 | 64.86 +- 0.17 |
| 16 | 3 | 64.21, 63.18, 62.86 | 63.42 +- 0.71 | 64.77, 64.15, 63.88 | 64.27 +- 0.46 |
| 16 | 4 | 59.07, 59.09, 62.20 | 60.12 +- 1.80 | 62.95, 59.43, 59.36 | 60.58 +- 2.05 |
| 16 | 5 | 73.67, 69.33, 69.01 | 70.67 +- 2.60 | 74.69, 73.59, 74.21 | 74.16 +- 0.55 |
| 16 | 6 | 72.00, 69.17, 69.15 | 70.11 +- 1.64 | 77.82, 74.98, 73.76 | 75.52 +- 2.08 |
| 32 | 1 | 216.06, 223.58, 226.57 | 222.07 +- 5.42 | 223.79, 226.37, 219.95 | 223.37 +- 3.23 |
| 32 | 2 | 93.75, 95.81, 95.93 | 95.16 +- 1.23 | 98.06, 98.69, 96.35 | 97.70 +- 1.21 |
| 32 | 3 | 199.36, 201.58, 201.57 | 200.84 +- 1.28 | 203.68, 204.91, 199.77 | 202.79 +- 2.68 |
| 32 | 4 | 75.41, 72.56, 71.86 | 73.28 +- 1.88 | 76.38, 79.74, 76.16 | 77.43 +- 2.01 |
| 32 | 5 | 88.15, 87.52, 85.62 | 87.10 +- 1.32 | 97.53, 97.40, 93.39 | 96.11 +- 2.35 |
| 32 | 6 | 86.64, 85.66, 84.26 | 85.52 +- 1.20 | 96.00, 93.32, 93.81 | 94.38 +- 1.43 |

The first run of every step is included in the mean. CSV column `first_run_excess` = rep1 / mean(rep2, rep3) - 1.
Possible warm-up (first run more than 4% slower than the other two):

| Set | Step | First-run excess |
|---|---|---|
| tuned n=16 | 5 | +6.5% |
| tuned n=16 | 6 | +4.1% |
| tuned n=32 | 4 | +4.4% |
| simple n=16 | 4 | +6.0% |
| simple n=16 | 6 | +4.6% |

Excluding rep1 would not change any conclusion below. Tuned n=32 step1 shows the opposite pattern:
its first run is the fastest (-4.0%), giving the largest spread of any set (std 2.4% of the mean).

## Speedup (mean wall time)

| n | Step | Tuned vs step1 | Tuned vs step4 | Simple vs step1 | Simple vs step4 |
|---|---|---|---|---|---|
| 16 | 2 | 0.976 | | 1.033 | |
| 16 | 3 | 1.036 | | 1.043 | |
| 16 | 4 | 1.093 | 1.000 | 1.106 | 1.000 |
| 16 | 5 | 0.930 | 0.851 | 0.904 | 0.817 |
| 16 | 6 | 0.937 | 0.858 | 0.887 | 0.802 |
| 32 | 2 | 2.334 | | 2.286 | |
| 32 | 3 | 1.106 | | 1.102 | |
| 32 | 4 | 3.031 | 1.000 | 2.885 | 1.000 |
| 32 | 5 | 2.550 | 0.841 | 2.324 | 0.806 |
| 32 | 6 | 2.597 | 0.857 | 2.367 | 0.820 |

GPU PETSc (steps 5-6) is still slower than CPU PETSc with the same Kokkos assembly and GPU NEML2 (step 4),
under both AMG configurations and at both mesh sizes.

## Linear solver

| n | AMG | Steps 1-4: KSP its total / median per solve / max / contraction per it | Steps 5-6: KSP its total / median / max / contraction |
|---|---|---|---|
| 16 | tuned | 5335 / 57 / 63 / 0.665 | ~22818 / 221.5 / 367 / 0.901 |
| 16 | simple | 4037 / 44 / 50 / 0.586 | ~31170 / 303.5 / 953 / 0.927 |
| 32 | tuned | ~5543 / 58.5 / 64 / 0.672 | ~28842 / 276.5 / 617 / 0.920 |
| 32 | simple | ~4897 / 52 / 115 / 0.638 | ~42958 / 415.5 / 868 / 0.946 |

Notes:
- 100 linear solves per run.
- Contraction per iteration = (|r_end| / |r_0|)^(1/its), median over solves. Per-solve statistics are from rep1.
- GPU iteration counts vary by about 1 between repetitions, and step5/step6 differ by about 1. hypre's device
  setup is not bitwise deterministic; the CPU counts are, apart from the 1-iteration step1/step3 difference at n=32.

PETSc -log_view times (s, mean of 3):

| n | AMG | KSPSolve step1 / step4 / step5 / step6 | PCSetUp / PCApply step1 | PCSetUp / PCApply step4 |
|---|---|---|---|---|
| 16 | tuned | 1.27 / 1.80 / 11.46 / 11.13 | 0.124 / 0.710 | 0.148 / 1.109 |
| 16 | simple | 1.64 / 2.14 / 15.86 / 15.60 | 0.267 / 1.172 | 0.327 / 1.615 |
| 32 | tuned | 4.38 / 6.60 / 17.83 / 17.74 | 0.410 / 2.927 | 0.543 / 4.773 |
| 32 | simple | 6.94 / 10.12 / 26.86 / 26.53 | 0.939 / 5.657 | 1.258 / 8.487 |

PCSetUp and PCApply print `n/a` in the GPU runs (steps 5-6), so they are N/A in the CSV. On the GPU only
KSPSolve and SNESSolve are timed, and KSPSolve includes the PC setup.

## Old vs new AMG

- **GPU PETSc (steps 5-6) improves:**
  - KSP iterations: -27% (n=16), -33% (n=32).
  - GPU KSPSolve: 15.9 -> 11.5 s (n=16), 26.9 -> 17.8 s (n=32).
  - Wall time: -3.5 / -5.4 s for steps 5 / 6 at n=16; -9.0 / -8.9 s at n=32.
  - Step 5/6 vs step4: 0.80-0.82 -> 0.84-0.86.
- **CPU PETSc (steps 1-4):** iterations increase (+32% at n=16, +13% at n=32), but each iteration is cheaper.
  - PC setup halves; PC apply falls 30-45%.
  - CPU KSPSolve therefore drops: step4 2.14 -> 1.80 s (n=16), 10.12 -> 6.60 s (n=32).
  - Step4 wall time: 60.6 -> 60.1 s (n=16, within noise), 77.4 -> 73.3 s (n=32).
- **The gap remains:** steps 5-6 still take 4.3x (n=16) and 5.2x (n=32) as many KSP iterations as the CPU steps.
  - The step 5 - step 4 wall-time difference (10.6 s at n=16, 13.8 s at n=32) is mostly the KSPSolve
    difference (9.7 s and 11.2 s).
  - The likely cause is the remaining device defaults in the effective-AMG table: PMIS, l1-Jacobi and no
    CF-relaxation on the GPU, against Falgout, SOR/Jacobi and CF on the CPU.
  - No experiment has isolated this yet, so it is unverified.
- **Session drift:** step1 contains no GPU PETSc, yet differs by -1.9% (n=16) and -0.6% (n=32) between the
  two sessions. Old/new wall-time differences smaller than about 2% are not significant.

## NEML2, GPU activity, memory

| Quantity | n=16 | n=32 |
|---|---|---|
| NEML2 time, CPU NEML2 (steps 1, 3) | ~50-54 s | ~180 s |
| NEML2 time, GPU NEML2 (steps 2, 4-6) | ~50-54 s | ~53-54 s |
| CUDA kernel time, tuned (Nsight), steps 2 / 3 / 4 / 5 / 6 | 8.6 / 0.6 / 9.2 / 14.7 / 15.0 s | step 5 21.8 s, step 6 22.2 s |
| H2D / D2H, tuned step 5 | 0.060 / 0.129 s | 0.070 / 0.157 s |
| H2D / D2H, tuned step 6 | 0.069 / 0.103 s | 0.078 / 0.124 s |

- At n=16, NEML2 takes about the same time on CPU and GPU. Because NEML2 dominates the wall time,
  GPU NEML2 gives no gain at this size.
- At n=32, GPU NEML2 is 3.4x faster, which accounts for most of the step2 and step4 speedups.

Peak GPU0 memory per run (nvidia-smi at 0.5 s, attributed with `run.log` timestamps; idle baseline 6 MiB):

| n | step1 | step2 | step3 | step4 | step5 | step6 |
|---|---|---|---|---|---|---|
| 16 | 214 | 360 | 530 | 614 | 884 | 4722 MiB |
| 32 | 214 | 612 | 554 | 890 | 1170 | 5004 MiB |

- **Constant memory:** peak equals median in every run, so memory is allocated up front and does not grow.
- **CUDA context in step1:** step1 holds a 214 MiB CUDA context even though it computes only on the host;
  the executable initializes the device at startup.
- **Step 6 offset:** step 6 uses about 3.8 GiB more than step 5. The offset is the same at n=16 and n=32
  and under the simple AMG (simple n=32 step 6: 5002 MiB), so it does not scale with the problem and is not
  caused by the AMG options. Likely sources are the MATHYPRE/cuda-vector path or a device pool reserved
  at initialization; the cause is not determined.

## Anomalies and data gaps

- **Effective AMG differs between CPU and GPU** (effective-AMG table above): the step 1-4 vs step 5-6
  comparison is not a like-for-like preconditioner comparison.
- **Twice the nonzeros in Kokkos steps:** the Kokkos steps store 138,988 matrix nonzeros against 68,380
  for the host steps at n=32. The Kokkos assembly keeps the whole allocated pattern, including explicit
  zeros; the host matrix is compacted.
  - KSP iterations are identical, but CPU PCSetUp/PCApply are about 1.3x/1.6x slower in step3/step4 than
    in step1/step2.
  - This confounds step1 vs step4. It does not affect steps 4 vs 5 vs 6, which share the same pattern.
- **Nsight:**
  - All 12 tuned profiles imported.
  - In the old simple n=32 set the step5 profile failed to import, so its GPU columns are N/A.
  - The old figures in `superseded_old_analyzer/` (older analyzer) show H2D/D2H as 0: their regex did not match the
    Nsight 2023.4 names "[CUDA memcpy Host-to-Device]" / "Device-to-Host". Fixed in `analyze_results.py`;
    the four files above use the fixed parser. The old files were left untouched; the raw runs behind
    `superseded_old_analyzer/fbar_off_simple_amg_n16.png/.csv` (F-bar OFF, simple AMG, n=16) are in
    `../raw/fbar_off_simple_amg_n16/`.
- **MatConvert:** steps 1-5 call MatConvert 100 times, once per PC setup (aij/aijkokkos -> HYPRE). Step 6
  assembles MATHYPRE directly via COO, has no MatConvert event (N/A in the CSV), and plots as 0.
- **n=64 stopped:**
  - Kept: step1_rep1 (complete) and a partial run renamed `step1_rep2_killed.*`, both in
    `../raw/fbar_simple_amg_n64_stopped/`.
  - Both runs show deterministic cutbacks at timesteps 10 and 12, so the killed run ends at t = 4.75 in
    20 steps.
  - No n=64 figures were produced.
- No fbar_simple_amg_n128 figure was produced because no complete n=128 F-bar set exists. The partial run is kept
  for debugging only in `../raw/fbar_simple_amg_n128_incomplete/` (see its `INCOMPLETE.md`).

## Changes made

- `run_benchmarks.sh`: `AMG=simple|tuned` switch (default tuned) that adds the five BoomerAMG options to
  all three PETSc argument sets.
- `analyze_results.py`:
  - New CSV columns: effective-AMG columns from -ksp_view; KSP iterations per solve and contraction;
    PCApply; final time, timestep count and failed timesteps; first_run_excess; speedup vs step1 and step4.
  - Fixed the H2D/D2H regex.
  - Figure changes: speedup panel with vs-step1 and vs-step4 bars; corrected titles.
  - New warnings: high spread, first-run excess, cutbacks, final-time mismatch.

No input physics, tolerances or gold files changed. Nothing committed.
