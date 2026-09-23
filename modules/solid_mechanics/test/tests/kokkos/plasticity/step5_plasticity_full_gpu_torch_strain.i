# Step 5 inherits the complete step 4 configuration and changes only PETSc to GPU execution.
# run_benchmarks.sh selects PETSc AIJKokkos and Kokkos vectors externally.
!include step4_plasticity_gpu_neml2_kokkos_cpu_petsc.i
