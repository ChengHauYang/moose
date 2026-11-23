# PetscJacobianTester input for ADGeneralizedKelvinVoigtModel + ADComputeLinearViscoelasticStress.
# Uses the same geometry and loading as ad_kelvin_voigt_small_strain.i.
# The test runs at least 3 steps (num_steps = 4) so the viscous history update
# (skipped on step 1) is exercised before the Jacobian check.

[Mesh]
  type = GeneratedMesh
  dim = 3
  nx = 1
  ny = 1
  nz = 1
  elem_type = HEX8
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[Kernels]
  [stress_div_x]
    type = ADStressDivergenceTensors
    variable = disp_x
    displacements = 'disp_x disp_y disp_z'
    component = 0
  []
  [stress_div_y]
    type = ADStressDivergenceTensors
    variable = disp_y
    displacements = 'disp_x disp_y disp_z'
    component = 1
  []
  [stress_div_z]
    type = ADStressDivergenceTensors
    variable = disp_z
    displacements = 'disp_x disp_y disp_z'
    component = 2
  []
[]

[BCs]
  [symmy]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [symmz]
    type = DirichletBC
    variable = disp_z
    boundary = back
    value = 0
  []
  # Prescribed displacement (not Neumann) keeps the residual bounded when
  # -snes_force_iteration drives extra Newton steps past convergence.
  [disp_x]
    type = DirichletBC
    variable = disp_x
    boundary = right
    value = 0.001
  []
[]

[Materials]
  [kelvin_voigt]
    type = ADGeneralizedKelvinVoigtModel
    creep_modulus = '10e9 10e9'
    creep_viscosity = '1 10'
    poisson_ratio = 0.2
    young_modulus = 10e9
  []
  [strain]
    type = ADComputeSmallStrain
    displacements = 'disp_x disp_y disp_z'
  []
  [stress]
    type = ADComputeLinearViscoelasticStress
  []
[]

[UserObjects]
  [update]
    type = ADLinearViscoelasticityManager
    viscoelastic_model = kelvin_voigt
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
  petsc_options_iname = '-snes_linesearch_type'
  petsc_options_value = 'basic'
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-10
  dtmin = 1e-3
  dt = 1
  num_steps = 4
[]
