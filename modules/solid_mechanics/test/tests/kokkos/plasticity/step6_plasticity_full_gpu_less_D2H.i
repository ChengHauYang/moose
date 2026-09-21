# Step 6 inherits the complete step 5 GPU configuration and changes only the strain transfer.
# The Kokkos strain buffer is exposed directly as a CUDA tensor instead of staging through host.
! step5_plasticity_full_gpu.i

[UserObjects/neml2_strain]
  moose_to_neml2_on_gpu = true
[]
