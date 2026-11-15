# Section C - 3D large-deformation path (handoverToCuda.md section C)

Environment
- MOOSE HEAD: f6edfc1c5c ([solid_mechanics] Fix F-bar stabilized TL Jacobian cofactor index);
  the part-3 F-bar fix below is committed at this SHA. Parts 1-2 were run at parent f0a0cd799b
  ([framework] Fix Kokkos-to-NEML2 tensor device labeling) and are unaffected by the one-index fix.
- NEML2 12f3d599, libtorch 2.13.0, PETSc 3.25.4, Kokkos 4.7.4, CUDA 12.4.0, opt, RTX A5000
- Harness: ./scripts/cuda/moose_cuda.sh (solid_mechanics rebuilt against the Section A fix)

Sequence run in order, stopping at the first failure (handoff rule).

## Part 1 - LD numerical baseline  => PASS
Note: --heavy runs ONLY heavy tests, so the single handoff command was split: heavy entries with
--heavy, non-heavy entries without. The $-anchored names select the baseline set (not the _padding,
_stabilized, or _rollback variants).
  test solid_mechanics 'neml2_input_gatherer.exact_kinematics$' -j 1 --heavy      => OK
  test solid_mechanics 'neml2_input_gatherer.(deformation_gradient|full_tensor_bridge|total_lagrangian_jacobian)$' -j 1
      deformation_gradient ....... OK   (CSVDiff)
      full_tensor_bridge ......... OK   (CSVDiff)
      total_lagrangian_jacobian .. OK   (PetscJacobianTester, NON-stabilized)
All on the default cuda device (CUDA Kokkos exec space; inputs set NEML2 device=cpu, so these also
exercise the Section A gather path CUDA Kokkos -> CPU NEML2). 3D total-Lagrangian Kokkos assembly,
full nonsymmetric rank-two/rank-four transfers, state advancement, and Jacobians qualified.

## Part 2 - LD Step 4 (CUDA NEML2 + CUDA Kokkos + CPU PETSc)  => PASS
  run solid_mechanics plasticity_LD/step4_plasticity_gpu_neml2_kokkos_cpu_petsc.i \
      N=8 Executioner/num_steps=2 Executioner/dt=5e-3 Outputs/exodus=false
  => Solve Converged, 2 steps, final nonlinear |R| = 3.64e-10 (finite residuals).
Nsight (profile.sh ld_step4_sectionC, --compute-device=cuda) confirms GPU execution post-fix:
83 distinct CUDA kernels total = 13 Kokkos cuda_parallel_launch compute kernels + 2 one-time
Kokkos/desul runtime init kernels (query_cuda_kernel_arch, init_lock_arrays) + 57 NEML2/torch
at::native CUDA kernels + 11 cuBLAS/cuSOLVER kernels (gemm/trsm/getrf/laswp/reduction) that torch
dispatches for the dense constitutive solve.
Step 4 uses TorchDeformationGradient, so it qualifies the deployed LD CUDA stack (not the direct
Kokkos input backend, which Sections A and D cover).

## Part 3 - stabilized / F-bar baseline  => PASS (after fixing a pre-existing F-bar cofactor typo)
  test solid_mechanics 'neml2_input_gatherer.(total_lagrangian_jacobian_stabilized|total_lagrangian_jacobian|fbar_cp_jacobian)$' -j 4           [cuda]  => 3 passed
  test solid_mechanics 'neml2_input_gatherer.(total_lagrangian_jacobian_stabilized|total_lagrangian_jacobian|fbar_cp_jacobian)$' --compute-device=cpu -j 4  => 3 passed
  test solid_mechanics 'neml2_input_gatherer.exact_kinematics_stabilized$' -j 1 --heavy                [cuda]  => 1 passed
  test solid_mechanics 'neml2_input_gatherer.exact_kinematics_stabilized$' --compute-device=cpu -j 1 --heavy    => 1 passed
      total_lagrangian_jacobian_stabilized . OK  (PetscJacobianTester, now below ratio_tol=1e-7; was 2.8%)
      total_lagrangian_jacobian ............ OK  (non-stabilized, unaffected)
      fbar_cp_jacobian ..................... OK  (run_sim=true, CP model)
      exact_kinematics_stabilized .......... OK

### total_lagrangian_jacobian_stabilized - root cause and fix
Initial failure: ||J - Jfd||_F/||J||_F = 0.0282 (2.8%), ||J - Jfd||_F = 344.76, vs ratio_tol = 1e-7.
Device-independent (identical on cpu and cuda). Introduced by commit 1123ca5dc9 ("kokkos: total-mode
F-bar for the total-Lagrangian NEML2 path"), NOT by the Section A device-labeling fix (the input uses
TorchDeformationGradient, not the Kokkos input-backend gatherers).

Root cause: a single-index typo in the per-qp inverse-transpose of F in
modules/solid_mechanics/include/kokkos/materials/KokkosComputeLagrangianStressCustomPK2.h:162.
  before: Cof[2][1] = -(F[0][0]*F[1][2] - F[0][2]*F[2][0]);   // F[2][0] is wrong
  after : Cof[2][1] = -(F[0][0]*F[1][2] - F[0][2]*F[1][0]);   // F[1][0] is correct
The (2,1) cofactor is -(F00*F12 - F02*F10). The same cofactor in the verified-correct F_avg inverse in
KokkosComputeFbarAverage.h:100 uses F[1][0], confirming the stress file's F[2][0] was the error. The
corrupted entry is Fust_invT[2][1] = F^{-T}[2][1], which feeds ONLY the F-bar local correction term
-(gamma/3)*A_iJ*F^{-T}[k][L] at (k,L)=(2,1); hence the error appears only when stabilize_strain=true,
is device-independent, and is absent from the non-stabilized path.

Numerical isolation (independent NumPy reference, finite-difference of the full element residual
including the non-local cross-qp F_avg coupling): the analytic tangent with the FIXED cofactor matches
FD to 1.4e-9 (central-difference floor); with the BUGGY cofactor it deviates by 7.5e-3 (same O(%)
structural class as the 2.8% seen in-test; magnitude scales with the deformation). This confirms the
typo is the sole defect and the rest of the F-bar tangent (A tensor, gamma/3, local PK2->PK1 chain with
geometric term, and the non-local rank-1 assembly) is correct.
Reference script: cuda-qualification/fbar_check.py (standalone NumPy, no deps beyond numpy;
run `python3 -I fbar_check.py`). It is a development aid, not a MOOSE test; the PetscJacobianTester
test total_lagrangian_jacobian_stabilized remains the integration regression for this fix.

Fix scope: one index on one line; no gold files changed, no tolerances loosened, no other code touched.

Execution devices (both runs are genuine, not a CPU fallback of a CUDA binary):
- Kokkos execution/memory space is fixed at compile time (KokkosHeader.h:34-55); on this box it is
  always ::Kokkos::Cuda/CudaSpace, so the F-bar material and TL kernel (KokkosComputeFbarAverage,
  KokkosComputeLagrangianStressCustomPK2, KokkosTotalLagrangianStressDivergence) run on the GPU in
  BOTH the cuda and the --compute-device=cpu runs.
- The stabilized input (total_lagrangian_jacobian_stabilized.i) pins NEML2 device=cpu, so the NEML2
  stress/tangent evaluate on the host via libtorch and are retrieved through the Section-A device
  contract. --compute-device=cpu additionally moves PETSc/libtorch to the host; --compute-device=cuda
  keeps libtorch on the GPU. The cofactor fix lives in the Kokkos (GPU) material, which is why the
  before/after result is identical on both device settings.

## Status
Section C fully PASS on cpu and cuda (parts 1, 2, 3), GPU execution confirmed in parts 1-2. The part-3
F-bar stabilized-Jacobian failure was root-caused to a pre-existing cofactor typo in the F-bar stress
material and fixed with a one-index change, restoring agreement with the finite-difference Jacobian at
the original 1e-7 tolerance. Fix committed as f6edfc1c5c (one-index change, reviewed and approved).
