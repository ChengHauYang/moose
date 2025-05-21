[Problem]
  # default_block = '0 2'
  default_block = '0'
[]

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

[Mesh]
  [gmg]
    type = FileMeshGenerator
    #file = "cube_cylinder.msh"
    file = "cube_cylinder_prism.msh"
  []

  [subdomain1]
    type = SubdomainInterceptedGenerator
    input = 'gmg'
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    lambda = 0.5
    outer_boundary = false
    function = '(y-0.3)^2+(z-1.5)^2-0.15^2'
  []

  # add_subdomain_ids = 2 # activated elements
  use_displaced_mesh = false
[]

[Variables]
  [T]
    order = FIRST
  []
[]

[SpatioTemporalHeat]
  path_file = 'InsideTriPt_filled_horizontal_lines.csv'
  ## for path
  verbose = true
  ## for esm
  # target_subdomain = 2
  target_subdomain = 0
  radius = 0.045
  execute_on_esm = 'TIMESTEP_BEGIN'
  # block = '1 2'
  block = '0 1'
  # within_elem_test = true
  # execute_on_esm = 'TIMESTEP_END'
  inactive_subdomain_ID = 1
  ic_strategy = "IC_EXTRAPOLATE_FIRST_LAYER"
  ## for heat source
  power = 1
  a = 0.2
  b = 0.1
  efficiency = 1
  scale = 1
  ## for kernel
  heat_variable = T
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        add_variables = true
        strain = FINITE
        automatic_eigenstrain_names = true
        generate_output = 'vonmises_stress'
        # block = '0 2'
      []
    []
  []
[]

[Materials]
  [thermal]
    type = HeatConductionMaterial
    thermal_conductivity = 45.0
    specific_heat = 0.5
  []
  [density]
    type = GenericConstantMaterial
    prop_names = 'density'
    prop_values = 8000.0
  []
  [elasticity]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1e9
    poissons_ratio = 0.0
  []
  [expansion1]
    type = ComputeThermalExpansionEigenstrain
    temperature = T
    thermal_expansion_coeff = 1e-7
    stress_free_temperature = 0
    eigenstrain_name = thermal_expansion
  []
  [stress]
    type = ComputeFiniteStrainElasticStress
  []
[]

[Kernels]
  [heat_conduction]
    type = HeatConduction
    variable = T
  []
  [time_derivative]
    type = HeatConductionTimeDerivative
    variable = T
  []
[]

# [AuxVariables]
#   [vonmises_stress]
#     order = CONSTANT
#     family = MONOMIAL
#   []
# []

# [AuxKernels]
#   [vonmises_stress]
#     type = RankTwoScalarAux
#     variable = vonmises_stress
#     rank_two_tensor = stress
#     scalar_type = VonMisesStress
#     execute_on = timestep_end
#   []
# []

[BCs]
  [bottom]
    type = DirichletBC
    variable = T
    boundary = bottom
    value = 0
  []

  [anchor_x]
    type = DirichletBC
    variable = disp_x
    # boundary = 'left right top bottom'
    boundary = 'bottom'
    value = 0.0
  []
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    # boundary = 'left right top bottom'
    boundary = 'bottom'
    value = 0.0
  []
  [anchor_z]
    type = DirichletBC
    variable = disp_z
    # boundary = 'left right top bottom'
    boundary = 'bottom'
    value = 0.0
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_max_its = 100
  nl_rel_tol = 1e-5
  nl_abs_tol = 1e-7
  dt = 1
  end_time = 1600
[]

[Outputs]
  exodus = true
[]
