# Step 7 inherits the complete step 6 GPU configuration and changes only the strain transfer.
# The Kokkos strain buffer is exposed directly as a CUDA tensor instead of staging through host.
!include step6_plasticity_full_gpu_host_staged_strain.i

[UserObjects/neml2_strain]
  moose_to_neml2_on_gpu = true
[]
