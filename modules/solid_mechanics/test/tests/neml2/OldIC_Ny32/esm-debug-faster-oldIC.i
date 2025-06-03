#neml2_input = rate_independent_plasticity_isoharden
#neml2_input = rate_independent_plasticity_perfect
#neml2_input = rate_independent_plasticity_isoharden_easy
neml2_input = viscoplasticity_isoharden

nx = 36
ny = 24
number_elements = 10

[Problem]
  kernel_coverage_check = false
  material_coverage_check = false
  #boundary_restricted_node_integrity_check = false
  #material_dependency_check = false
  #boundary_restricted_elem_integrity_check = false
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
    xmin = 0
    xmax = 3
    ymin = 0
    ymax = 2
  []
  [block_left]
    type = SubdomainBoundingBoxGenerator
    input = gmg
    block_id = 0
    block_name = material_left
    bottom_left = '0 0 0'
    top_right = '1.25 2.0 0'
  []
  [block_right]
    type = SubdomainBoundingBoxGenerator
    input = block_left
    block_id = 1
    block_name = material_right
    bottom_left = '1.75 0 0'
    top_right = '3.0 2.0 0'
  []
  [block_middle]
    type = SubdomainBoundingBoxGenerator
    input = block_right
    block_id = 2
    block_name = material_null
    bottom_left = '1.25 0 0'
    top_right = '1.75 2.0 0'
  []
  [block_middle_new]
    type = SubdomainBoundingBoxGenerator
    input = block_middle
    block_id = 3
    block_name = material_middle
    bottom_left = '1.25 1.5 0'
    top_right = '1.75 2.0 0'
  []
  use_displaced_mesh = false
[]

[MeshModifiers]
  # [UserObjects]
  [esm]
    type = RowElementModifier
    subdomain_id_change_from = 2
    subdomain_id_change_to = 3
    number_of_elements = ${number_elements}
    x_min = 1.25
    x_max = 1.75
    y_min = 0
    y_max = 2
    change_one_row = true
    execute_on = 'INITIAL TIMESTEP_END'
    #unsolved_blocks = '2'
    #ic_strategy = "IC_EXTRAPOLATE_FIRST_LAYER"
  []
[]

# [UserObjects]
# [line_filling]
#   type = RowElementModifier
#   subdomain_id_change_from = 2
#   subdomain_id_change_to = 3
#   number_of_elements = ${number_elements}
#   x_min = 1.25
#   x_max = 1.75
#   y_min = 0
#   y_max = 2
#   change_one_row = true
#   execute_on = 'INITIAL TIMESTEP_BEGIN'
# []
# []

[Variables]
  [disp_x]
    block = '0 1 3'
  []
  [disp_y]
    block = '0 1 3'
  []
[]

[AuxVariables]
  [ep]
    #  Auxiliary Kernels override computeValue() instead of computeQpResidual().  Aux Variables
    #  are calculated either one per elemenet or one per node depending on whether we declare
    #  them as "Elemental (Constant Monomial)" or "Nodal (First Lagrange)".  No changes to the
    #  source are necessary to switch from one type or the other.
    order = CONSTANT
    family = MONOMIAL
    block = '0 1 3'
    [AuxKernel]
      type = MaterialRealAux
      property = equivalent_plastic_strain
      block = '0 1 3'
      execute_on = 'INITIAL TIMESTEP_END'
    []
  []
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = SMALL
        new_system = true
        formulation = TOTAL
        volumetric_locking_correction = true
        block = '0 1 3'
      []
    []
  []
[]

[NEML2]
  input = '../models/${neml2_input}.i'
  verbose = true
  device = 'cpu'

  moose_input_types = 'MATERIAL     MATERIAL     POSTPROCESSOR POSTPROCESSOR MATERIAL     MATERIAL'
  moose_inputs = '     neml2_strain neml2_strain time          time          neml2_stress equivalent_plastic_strain'
  neml2_inputs = '     forces/E     old_forces/E forces/t      old_forces/t  old_state/S  old_state/internal/ep'

  moose_output_types = 'MATERIAL     MATERIAL'
  moose_outputs = '     neml2_stress equivalent_plastic_strain'
  neml2_outputs = '     state/S      state/internal/ep'

  moose_derivative_types = 'MATERIAL'
  moose_derivatives = 'neml2_jacobian'
  neml2_derivatives = 'state/S forces/E'

  #    moose_input_types = 'MATERIAL     POSTPROCESSOR POSTPROCESSOR MATERIAL'
  #    moose_inputs = '     neml2_strain time          time          plastic_strain'
  #    neml2_inputs = '     forces/E     forces/t      old_forces/t  old_state/internal/Ep'
  #
  #    moose_output_types = 'MATERIAL     MATERIAL'
  #    moose_outputs = '     neml2_stress plastic_strain'
  #    neml2_outputs = '     state/S      state/internal/Ep'
  #
  #    moose_derivative_types = 'MATERIAL'
  #    moose_derivatives = 'neml2_jacobian'
  #    neml2_derivatives = 'state/S forces/E'

  [A]
    model = 'model'
    block = 'material_left material_middle material_right'
    #copy_value2old = "NewToOld"
  []
  #  [B]
  #    model = 'model'
  #    block = 'material_right'
  #  []
  #  [C]
  #    model = 'model'
  #    block = 'material_middle'
  #  []
[]

[Materials]
  [convert_strain]
    type = RankTwoTensorToSymmetricRankTwoTensor
    from = 'mechanical_strain'
    to = 'neml2_strain'
    block = '0 1 3'
  []
  [stress]
    type = ComputeLagrangianObjectiveCustomSymmetricStress
    custom_small_stress = 'neml2_stress'
    custom_small_jacobian = 'neml2_jacobian'
    block = '0 1 3'
  []
[]

[Functions]
  [displacement_with_time]
    type = ParsedFunction
    #expression = '0.008*t + 1'
    expression = '0.008*t'
  []
[]

[BCs]
  [anchor_x]
    #Anchors the left side against deformation in the x-direction
    type = DirichletBC
    variable = disp_x
    boundary = 'left'
    value = 0.0
  []
  [anchor_y]
    #Anchors the left side against deformation in the y-direction
    type = DirichletBC
    variable = disp_y
    boundary = 'left'
    value = 0.0
  []
  [displacement_x_right]
    #Anchors the left side against deformation in the x-direction
    type = FunctionDirichletBC
    variable = disp_x
    boundary = 'right'
    function = displacement_with_time
    preset = false
  []
  # [displacement_y_right]
  #   #Anchors the left side against deformation in the y-direction
  #   type = DirichletBC
  #   variable = disp_y
  #   boundary = 'right'
  #   value = 0.0
  # []
[]

#[Executioner]
#  type = Transient
#  solve_type = NEWTON
#  petsc_options_iname = '-pc_type'
#  petsc_options_value = 'lu'
#  automatic_scaling = true
#  dt = 1
#  end_time = 36
#  residual_and_jacobian_together = true
#[]

[Executioner]
  solve_type = NEWTON
  #solve_type = PJFNK
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  #petsc_options_iname = '-ksp_type -pc_type -sub_pc_type'
  #petsc_options_value = 'bcgs  jacobi lu'
  nl_max_its = 12
  nl_rel_tol = 1e-6
  nl_abs_tol = 1e-10
  type = Transient
  dt = 0.01
  end_time = 36
  automatic_scaling = true
  residual_and_jacobian_together = true
  line_search = none
  # abort_on_solve_fail = true
  dtmin = 0.0025
[]

[Postprocessors]
  [Ux]
    type = ElementAverageValue
    variable = disp_x
    block = '3'
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [Uy]
    type = ElementAverageValue
    variable = disp_y
    block = '3'
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [ep_post]
    type = ElementAverageValue
    variable = ep
    block = '3'
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [time]
    type = TimePostprocessor
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Outputs]
  interval = 1
  exodus = true
  csv = true
[]
