# AD generalized Kelvin-Voigt model, total small strain, constant Neumann load.
# Verifies that creep strain grows from zero over multiple timesteps (observable
# creep under a constant surface traction), and that the AD Jacobian is exact.

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
  [symmx]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [symmz]
    type = DirichletBC
    variable = disp_z
    boundary = back
    value = 0
  []
  [axial_load]
    type = NeumannBC
    variable = disp_x
    boundary = right
    value = 10e6
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
  nl_rel_tol = 1e-10
  nl_abs_tol = 1e-10
  dt = 1
  num_steps = 5
[]

[Postprocessors]
  [disp_x_right]
    type = SideAverageValue
    variable = disp_x
    boundary = right
  []
  [creep_strain_xx]
    type = ElementAverageValue
    variable = creep_strain_xx
  []
[]

[AuxVariables]
  [creep_strain_xx]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [creep_strain_xx]
    type = ADRankTwoAux
    variable = creep_strain_xx
    rank_two_tensor = creep_strain
    index_i = 0
    index_j = 0
    execute_on = timestep_end
  []
[]

[Outputs]
  csv = true
[]
