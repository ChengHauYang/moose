# sczm_tl_3d_kokkos.i with its Kokkos traction-separation law replaced by the same law evaluated by
# NEML2: the Kokkos displacement jump is gathered into NEML2 and the NEML2 traction and its jump
# derivative are retrieved as Kokkos interface material properties.
diffusion_type = KokkosDiffusion
dirichlet_type = KokkosDirichletBC
traction_type = KokkosPureElasticTractionSeparation
use_kokkos = true
no_shifted = false
file_base = sczm_tl_3d_neml2_traction_out

!include sczm_tl_3d_common.i

[Materials]
  inactive = 'local_traction'
[]

[NEML2]
  input = 'linear_traction_neml2.i'
  eager = true
  [traction]
    model = 'czm'
    executor_name = 'neml2_traction'
    interface = 'Block0_Block1'
    volume = false
    input_backend = kokkos
    output_backend = kokkos
    interface_material_inputs = 'interface_displacement_jump'
    derivatives = 'interface_traction interface_displacement_jump dinterface_traction_djump'
    interface_output_side = primary
  []
[]
