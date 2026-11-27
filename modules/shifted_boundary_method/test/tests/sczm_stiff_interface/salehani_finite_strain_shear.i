# Salehani-Irani traction-separation law under shear, total-Lagrangian finite strain. The cohesive
# zone strain must also be set to FINITE, on whichever cohesive zone block is used.

[Physics/SolidMechanics/QuasiStatic/all]
  strain = FINITE
  formulation = TOTAL
  new_system = true
[]

[Materials]
  [elastic_stress]
    type = ComputeLagrangianLinearElasticStress
  []
  [czm]
    type = SalehaniIrani3DCTraction
    # The initial stiffness e * 500 / 1.36e-5 is about 1e8, 1e5 times the bulk Young's modulus
    normal_gap_at_maximum_normal_traction = 1.36e-5
    tangential_gap_at_maximum_shear_traction = 1.36e-5
    maximum_normal_traction = 500
    maximum_shear_traction = 300
    boundary = 'Block1_Block2'
  []
[]

[BCs]
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = 'left'
    value = 0
  []
  [right_x]
    type = DirichletBC
    variable = disp_x
    boundary = 'right'
    value = 0
  []
  [shear]
    type = FunctionDirichletBC
    variable = disp_y
    boundary = 'right'
    function = '-1e-2*t'
    preset = false
  []
[]
