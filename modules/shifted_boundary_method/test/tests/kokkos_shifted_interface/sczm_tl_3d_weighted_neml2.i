file_base = sczm_tl_3d_weighted_neml2_out

!include sczm_tl_3d_weighted_common.i

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
