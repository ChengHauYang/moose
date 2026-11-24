# Jacobian verification for the AD beam prescribed eigenstrain chain.
#
# A single-element cantilever beam (left end fully fixed) has a prescribed
# displacement eigenstrain eig_disp_x = 0.01 applied via
# ADComputeEigenstrainBeamFromVariable. The eigenstrain components are nonlinear
# variables with time-derivative equations, making the system nonsingular while
# preserving their AD derivatives. ADComputeIncrementalBeamStrain and
# ADComputeBeamResultants propagate those derivatives into forces and moments;
# ADStressDivergenceBeam assembles the residual through the
# computeOffDiagJacobian() AD path.
#
# PetscJacobianTester compares the assembled Jacobian against a finite-difference
# approximation, including the nonzero off-diagonal blocks coupling prescribed
# eigenstrain to the displacement and rotation residuals.

[Mesh]
  type = GeneratedMesh
  dim = 1
  nx = 1
  xmin = 0.0
  xmax = 1.0
  displacements = 'disp_x disp_y disp_z'
[]

[Variables]
  [disp_x]
    order = FIRST
    family = LAGRANGE
  []
  [disp_y]
    order = FIRST
    family = LAGRANGE
  []
  [disp_z]
    order = FIRST
    family = LAGRANGE
  []
  [rot_x]
    order = FIRST
    family = LAGRANGE
  []
  [rot_y]
    order = FIRST
    family = LAGRANGE
  []
  [rot_z]
    order = FIRST
    family = LAGRANGE
  []
[]

[Variables]
  [eig_disp_x]
    order = FIRST
    family = LAGRANGE
  []
  [eig_disp_y]
    order = FIRST
    family = LAGRANGE
  []
  [eig_disp_z]
    order = FIRST
    family = LAGRANGE
  []
  [eig_rot_x]
    order = FIRST
    family = LAGRANGE
  []
  [eig_rot_y]
    order = FIRST
    family = LAGRANGE
  []
  [eig_rot_z]
    order = FIRST
    family = LAGRANGE
  []
[]

[ICs]
  [eig_disp_x_ic]
    type = ConstantIC
    variable = eig_disp_x
    value = 0.01
  []
[]

[Kernels]
  [solid_disp_x]
    type = ADStressDivergenceBeam
    displacements = 'disp_x disp_y disp_z'
    rotations = 'rot_x rot_y rot_z'
    component = 0
    variable = disp_x
  []
  [solid_disp_y]
    type = ADStressDivergenceBeam
    displacements = 'disp_x disp_y disp_z'
    rotations = 'rot_x rot_y rot_z'
    component = 1
    variable = disp_y
  []
  [solid_disp_z]
    type = ADStressDivergenceBeam
    displacements = 'disp_x disp_y disp_z'
    rotations = 'rot_x rot_y rot_z'
    component = 2
    variable = disp_z
  []
  [solid_rot_x]
    type = ADStressDivergenceBeam
    displacements = 'disp_x disp_y disp_z'
    rotations = 'rot_x rot_y rot_z'
    component = 3
    variable = rot_x
  []
  [solid_rot_y]
    type = ADStressDivergenceBeam
    displacements = 'disp_x disp_y disp_z'
    rotations = 'rot_x rot_y rot_z'
    component = 4
    variable = rot_y
  []
  [solid_rot_z]
    type = ADStressDivergenceBeam
    displacements = 'disp_x disp_y disp_z'
    rotations = 'rot_x rot_y rot_z'
    component = 5
    variable = rot_z
  []
  [eig_disp_x_dt]
    type = ADTimeDerivative
    variable = eig_disp_x
  []
  [eig_disp_y_dt]
    type = ADTimeDerivative
    variable = eig_disp_y
  []
  [eig_disp_z_dt]
    type = ADTimeDerivative
    variable = eig_disp_z
  []
  [eig_rot_x_dt]
    type = ADTimeDerivative
    variable = eig_rot_x
  []
  [eig_rot_y_dt]
    type = ADTimeDerivative
    variable = eig_rot_y
  []
  [eig_rot_z_dt]
    type = ADTimeDerivative
    variable = eig_rot_z
  []
[]

[BCs]
  [fix_disp_x]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
  []
  [fix_disp_y]
    type = DirichletBC
    variable = disp_y
    boundary = left
    value = 0.0
  []
  [fix_disp_z]
    type = DirichletBC
    variable = disp_z
    boundary = left
    value = 0.0
  []
  [fix_rot_x]
    type = DirichletBC
    variable = rot_x
    boundary = left
    value = 0.0
  []
  [fix_rot_y]
    type = DirichletBC
    variable = rot_y
    boundary = left
    value = 0.0
  []
  [fix_rot_z]
    type = DirichletBC
    variable = rot_z
    boundary = left
    value = 0.0
  []
[]

[Materials]
  [elasticity]
    type = ComputeElasticityBeam
    youngs_modulus = 1e6
    poissons_ratio = 0.3
    shear_coefficient = 1.0
  []
  [strain]
    type = ADComputeIncrementalBeamStrain
    displacements = 'disp_x disp_y disp_z'
    rotations = 'rot_x rot_y rot_z'
    area = 0.5
    Iy = 0.01
    Iz = 0.01
    y_orientation = '0.0 1.0 0.0'
    eigenstrain_names = 'prescribed'
  []
  [stress]
    type = ADComputeBeamResultants
  []
  [prescribed]
    type = ADComputeEigenstrainBeamFromVariable
    displacement_eigenstrain_variables = 'eig_disp_x eig_disp_y eig_disp_z'
    rotational_eigenstrain_variables = 'eig_rot_x eig_rot_y eig_rot_z'
    eigenstrain_name = prescribed
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
  dt = 1.0
  num_steps = 1
[]

[Postprocessors]
  [disp_x]
    type = NodalExtremeValue
    variable = disp_x
    value_type = max
  []
[]

[Outputs]
  exodus = false
[]
