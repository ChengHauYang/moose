[Mesh]
  parallel_type = DISTRIBUTED
  use_displaced_mesh = false
  [base]
    type = FileMeshGenerator
    file = creepTest10Gr.exo
  []
  [boundary]
    type = SideSetsFromNormalsGenerator
    input = base
    normals = '-1  0  0
                1  0  0
                0 -1  0
                0  1  0
                0  0 -1
                0  0  1'
    new_boundary = 'x0 x1 y0 y1 z0 z1'
    fixed_normal = true
  []
[]

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
  large_kinematics = true
[]

[Physics/SolidMechanics/QuasiStatic]
  [all]
    strain = FINITE
    new_system = true
    formulation = TOTAL
    add_variables = true
    volumetric_locking_correction = false
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 1e5
    poissons_ratio = 0.3
  []
  [stress]
    type = ComputeLagrangianLinearElasticStress
  []
[]

[BCs]
  [x0]
    type = DirichletBC
    variable = disp_x
    boundary = x0
    value = 0
  []
  [y0]
    type = DirichletBC
    variable = disp_y
    boundary = y0
    value = 0
  []
  [z0]
    type = DirichletBC
    variable = disp_z
    boundary = z0
    value = 0
  []
  [y1]
    type = FunctionNeumannBC
    variable = disp_y
    boundary = y1
    function = '10 * t'
  []
[]

[Constraints]
  [x1]
    type = EqualValueBoundaryConstraint
    variable = disp_x
    secondary = x1
    penalty = 1e7
  []
  [y1]
    type = EqualValueBoundaryConstraint
    variable = disp_y
    secondary = y1
    penalty = 1e7
  []
  [z1]
    type = EqualValueBoundaryConstraint
    variable = disp_z
    secondary = z1
    penalty = 1e7
  []
[]

[Postprocessors]
  [strain]
    type = SideAverageValue
    variable = disp_y
    boundary = y1
    execute_on = TIMESTEP_END
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  dt = 1
  num_steps = 3
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-12
  nl_max_its = 20
  l_tol = 1e-12
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu mumps'
[]

[Outputs]
  [csv]
    type = CSV
    execute_on = FINAL
  []
[]
