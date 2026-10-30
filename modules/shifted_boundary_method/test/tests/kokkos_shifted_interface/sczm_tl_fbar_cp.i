# Merged after sczm_tl_fbar_host.i or sczm_tl_fbar_kokkos.i, this input replaces the elastic
# interface model with the stateful crystal-plasticity model of sczm_tl_fbar_cp_neml2.i and takes
# several time steps under fixed boundary displacements, so the viscoplastic interface state keeps
# relaxing and every step after the first starts from an advanced plastic state.
#
# The bulk takes the elastic constants of the crystal-plasticity model, and the cohesive stiffness
# is scaled with them by 1000. The boundary displacements are scaled down by 100, which keeps the
# elastic trial stress near the slip strength of 50, and the solve starts from zero displacement.

interface_model = cp_interface_model

[ICs]
  inactive = 'ux_left ux_right uy uz_left uz_right'
[]

[BCs]
  [ux_right]
    value = 0.0008
  []
  [uy_right]
    value = -0.0005
  []
  [uz_right]
    value = 0.0004
  []
[]

[NEML2]
  input = sczm_tl_fbar_cp_neml2.i
  [interface]
    output_variables = 'interface_pk1 plastic_deformation_gradient'
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
[]

[Materials]
  [local_traction]
    normal_stiffness = 1e4
    tangent_stiffness = 8e3
  []
  [initial_plastic_defgrad]
    type = GenericConstantRankTwoTensor
    tensor_name = initial_plastic_defgrad
    tensor_values = '1 1 1'
  []
[]

[Executioner]
  type = Transient
  num_steps = 3
  dt = 0.01
[]
