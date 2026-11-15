# plasticity_LD_sczm_grain_boundaries benchmark figures

Every figure is `<name>.png` with its per-step data in `<name>.csv`, and its raw runs (logs, `.time`,
exodus, CSV, Nsight profiles) are in `../raw/<name>/`. A name reads

    <fbar | fbar_off>_<AMG configuration>_amg_n<ny>[_<qualifier>]

- `fbar` / `fbar_off`: total-mode F-bar ON (`VARIANT=fbar` in `run_benchmarks.sh`) or OFF.
- AMG configuration (`AMG=` in `run_benchmarks.sh`):
  - `simple`: the step inputs' plain `gmres` + `hypre boomeramg`.
  - `tuned`: adds `max_iter 1, tol 0, interp ext+i, agg_nl 1, strong_threshold 0.6` to every step.
- `n<ny>`: mesh `ny` (nx = 3 ny / 2); n16 = 384 elements / 968 DOFs, n32 = 1536 elements / 3462 DOFs.
- Each figure shows the six steps (1 host, 2 host + GPU NEML2, 3 Kokkos, 4 Kokkos + GPU NEML2,
  5 + AIJKOKKOS, 6 + MATHYPRE), mean of 3 runs unless the qualifier says otherwise.

| Figure | Content |
|---|---|
| `fbar_tuned_amg_n16` | F-bar ON, tuned AMG, n=16 |
| `fbar_tuned_amg_n32` | F-bar ON, tuned AMG, n=32 |
| `fbar_simple_amg_n16` | F-bar ON, simple AMG, n=16 |
| `fbar_simple_amg_n32` | F-bar ON, simple AMG, n=32 |
| `fbar_gpu_amg_variants_n16` | F-bar ON, n=16, steps 5-6 only: tuned AMG plus CPU-style coarsening/relaxation on the GPU (see below) |

`amg_report_n16_n32.md` compares the tuned and simple sets; `fbar_gpu_amg_variants_n16.md` reports
the GPU AMG variant study.

`fbar_gpu_amg_variants_n16` keeps the tuned options and adds, on the GPU steps only,
`-pc_hypre_boomeramg_relax_type_all symmetric-SOR/Jacobi` and/or `-pc_hypre_boomeramg_coarsen_type HMIS`
(`EXTRA_PETSC_ARGS` in `run_benchmarks.sh`). It compares them with the tuned GPU default (PMIS +
l1-Jacobi) and the step 4 CPU PETSc reference. Its raw directory holds one subdirectory per variant
(`ssor`, `hmis_ssor`, `hmis`, each with `run.log` and `gpu0_samples.csv` from nvidia-smi), the
one-timestep smoke tests in `smoke/` (including the Falgout and CF-relaxation crashes), the driver
scripts, `cpustyle_compare.py` that made the figure, and `verify_gpu_amg_variants_n16.out` (solution
history vs tuned step1). The `*_full` directories are symlink sets (steps 1-4 from
`../raw/fbar_tuned_amg_n16`) used only by the plotting script.

## superseded_old_analyzer/

Made by an older `analyze_results.py`: H2D/D2H are drawn as 0 (Nsight name mismatch, since fixed),
there are no error bars or effective-AMG columns, and the KSP totals differ slightly from the current
parser. Kept for reference only; use the figures above.

| Figure | Former name | Content |
|---|---|---|
| `fbar_off_simple_amg_n16` | `comparison` | F-bar OFF, simple AMG, n=16, 3 runs (the only F-bar OFF figure) |
| `fbar_off_simple_amg_n16_draft.png` | `comparation.png` | Same data as above; earlier draft with shorter panel titles |
| `fbar_simple_amg_n16` | `comparison_n16` | Same runs as `../fbar_simple_amg_n16` |
| `fbar_simple_amg_n32_1rep` | `comparison_n32` | F-bar ON, simple AMG, n=32, 1 run per step |

The earlier `outputs_n16/n32` and `simple_amg_n16/n32` figures are the four current figures above,
renamed.

## Raw datasets without a figure

- `../raw/fbar_simple_amg_n64_stopped/`: n=64 stopped after step1 (one complete run, one killed run).
- `../raw/fbar_simple_amg_n128_incomplete/`: debugging only, see its `INCOMPLETE.md`. Do not use for
  performance comparisons or plots.
