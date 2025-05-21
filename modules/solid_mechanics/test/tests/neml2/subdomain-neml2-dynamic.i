neml2_input = elasticity_3

nx = 12
ny = 8
number_elements = 1
#number_elements = 0.25

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
    bottom_left = '1.25 1.0 0'
    top_right = '1.75 2.0 0'
  []
  use_displaced_mesh = false
[]

[MeshModifiers]
  [line_filling]
    type = RowElementModifier
    subdomain_id_change_from = 2
    subdomain_id_change_to = 3
    number_of_elements = ${number_elements}
    x_min = 1.25
    x_max = 1.75
    y_min = 0
    y_max = 2
    change_one_row = true
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = SMALL
        new_system = true
        add_variables = true
        formulation = TOTAL
        volumetric_locking_correction = true
        block = '0 1 3'
      []
    []
  []
[]

[NEML2]
  input = 'models/${neml2_input}.i'
  verbose = true
  [A]
    model = 'model1'
    block = 'material_left'
    device = 'cpu'

    moose_input_types = 'MATERIAL'
    moose_inputs = 'neml2_strain'
    neml2_inputs = 'forces/E'

    moose_output_types = 'MATERIAL'
    moose_outputs = 'neml2_stress'
    neml2_outputs = 'state/S'

    moose_derivative_types = 'MATERIAL'
    moose_derivatives = 'neml2_jacobian'
    neml2_derivatives = 'state/S forces/E'
  []
  [B]
    model = 'model2'
    block = 'material_right'
    device = 'cpu'

    moose_input_types = 'MATERIAL'
    moose_inputs = 'neml2_strain'
    neml2_inputs = 'forces/E'

    moose_output_types = 'MATERIAL'
    moose_outputs = 'neml2_stress'
    neml2_outputs = 'state/S'

    moose_derivative_types = 'MATERIAL'
    moose_derivatives = 'neml2_jacobian'
    neml2_derivatives = 'state/S forces/E'
  []
  [C]
    model = 'model3'
    block = 'material_middle'
    device = 'cpu'

    moose_input_types = 'MATERIAL'
    moose_inputs = 'neml2_strain'
    neml2_inputs = 'forces/E'

    moose_output_types = 'MATERIAL'
    moose_outputs = 'neml2_stress'
    neml2_outputs = 'state/S'

    moose_derivative_types = 'MATERIAL'
    moose_derivatives = 'neml2_jacobian'
    neml2_derivatives = 'state/S forces/E'
  []
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
    expression = '0.05*t + 1'
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
  []
  [displacement_y_right]
    #Anchors the left side against deformation in the y-direction
    type = DirichletBC
    variable = disp_y
    boundary = 'right'
    value = 0.0
  []
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
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  type = Transient
  dt = 1
  end_time = 8
  automatic_scaling = true
  residual_and_jacobian_together = true
[]

[Postprocessors]
[]

[Outputs]
  vtk = true
[]
