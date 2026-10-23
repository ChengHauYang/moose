# Section D - 3D SCZM direct Kokkos input (handoverToCuda.md section D)

Environment
- MOOSE HEAD: f6edfc1c5c ([solid_mechanics] Fix F-bar stabilized TL Jacobian cofactor index)
- NEML2 12f3d599, libtorch 2.13.0, PETSc 3.25.4, Kokkos 4.7.4, CUDA 12.4.0, opt, RTX A5000
- Harness: ./scripts/cuda/moose_cuda.sh (shifted_boundary_method rebuilt against the F-bar fix)

Tests: modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/tests
Each SCZM test declares compute_devices, and several come in a cpu fixture plus a cuda twin
(e.g. sczm_pk2_kokkos_jacobian [cpu] / sczm_pk2_kokkos_jacobian_cuda [cuda, NEML2/device=cuda]).
As in Section A, a test is only genuinely exercised under the device(s) it declares, so every item
was run under each declared device; a "[cuda/cpu not in compute devices] SKIP" of the other twin is
the expected, correct behavior (not a pass-by-skip). No skip here was caused by heavy, MPI slots, or
mesh mode.

## PK2 Jacobian + Action modes
  verify-equivalent: test shifted_boundary_method 'sczm_pk2_(kokkos_jacobian|action_modes)'
    CUDA (default device):
      sczm_pk2_kokkos_jacobian_cuda ....................... OK   (PetscJacobianTester, NEML2/device=cuda)
      (cpu twins skip: [cuda not in compute devices])
    CPU (--compute-device=cpu):
      sczm_pk2_kokkos_jacobian ............................ OK   (PetscJacobianTester)
      sczm_pk2_action_modes/directional_correction_inactive OK  (RunApp)
      sczm_pk2_action_modes/shifted_terms_inactive ........ OK   (RunApp)
      sczm_pk2_action_modes/host_active ................... OK   (RunException: host PK2->PK1 rejected)
PK2->PK1 conversion of distinct primary/neighbor full stress and tangent, all 3D displacement
couplings of the directional correction, at ratio_tol=1e-7. Action contract (converters created only
when the active directional correction consumes them; host path rejected) verified.

## Distributed PK2 Jacobian (2 ranks, distributed mesh)
  test shifted_boundary_method 'sczm_pk2_kokkos_jacobian' -j 2 -p2 --distributed-mesh
    CUDA: sczm_pk2_kokkos_jacobian_cuda ... OK      CPU: sczm_pk2_kokkos_jacobian ... OK
Interface elements and ghost-side neighbors split across MPI ranks; Jacobian still consistent.

## Primary/neighbor interface mapping (heterogeneous blocks)
  test shifted_boundary_method 'sczm_hetero_jacobian_blocks'
    CUDA ... OK      CPU ... OK
Confirms the directional-correction residual depends on primary stress in the element row and neighbor
stress in the neighbor row, with the NEML2 batch index keyed by the primary quadrature point and the
neighbor property written to the matching neighbor-storage qp, and no correction to the cross blocks.
This is the dedicated both-interface-sides property-mapping oracle.

## Stateful NEML2 regression (loading/unloading, history advancement)
  Serial:      test 'sczm_stateful_serial' -j 1
    CUDA: sczm_stateful_serial_cuda ........... OK   CPU: sczm_stateful_serial ........... OK
  Distributed: test 'sczm_stateful_distributed' -j 2 -p2 --distributed-mesh
    CUDA: sczm_stateful_distributed_cuda ...... OK   CPU: sczm_stateful_distributed ...... OK
Exodiff vs sczm_stateful_kokkos_out.e (abs_zero=1e-8, rel_err=1e-8). History advances only after
converged steps; distributed results match serial across the ghost-side neighbor partition.

## GPU execution proof (Nsight Systems)
profile.sh sczm_pk2_sectionD (sczm_pk2_kokkos.i, --compute-device=cuda NEML2/device=cuda
--allow-test-objects) -> cuda-qualification/sczm_pk2_sectionD.{nsys-rep,cuda-kernels.txt,run.log}.
Solve Converged (4 nonlinear iters, final |R| = 1.18e-16). GPU kernel summary (nsys stats
--report cuda_gpu_kern_sum): 34 distinct CUDA kernels total:
- 25 Kokkos CUDA cuda_parallel_launch compute kernels: SCZM interface assembly, stress divergence
  (Di...), FE interpolation (FE...), deformation-gradient gather into NEML2 - the Kokkos compute path
  runs on the GPU.
- 2 one-time Kokkos/desul runtime init kernels (Kokkos::Impl::query_cuda_kernel_arch,
  desul::init_lock_arrays_cuda_kernel) - the Kokkos CUDA backend itself initializes on-device.
- 7 NEML2/torch at::native CUDA kernels (gpu_kernel_impl, CatArrayBatchedCopy, ...) - NEML2 evaluates
  the primary and neighbor PK2 models on CUDA with outputs kept on-device for the zero-copy retriever.
This proves the whole 3D SCZM PK2 path (Kokkos gather -> NEML2 primary/neighbor eval -> Kokkos
PK2->PK1 + directional-correction assembly) executes on the device, not merely that it compiled.

Note: the first profiling attempt errored at setup with "KokkosPK2ToPK1PrimaryInterfaceOracle is not a
registered object" because sczm_pk2_kokkos.i uses a test-only oracle; the direct binary call needed
--allow-test-objects (the TestHarness supplies it via allow_test_objects=true). Added and re-profiled;
not a GPU-execution defect.

## Status
Section D: PASS. PK2 Jacobian (serial + 2-rank distributed), action-mode contract, primary/neighbor
property mapping, and the stateful loading/unloading regression all pass on both cpu and cuda at the
original 1e-7 Jacobian tolerances and 1e-8 Exodiff tolerances; GPU execution of the 3D SCZM PK2 path
confirmed by Nsight. No gold files changed, no tolerances loosened.
