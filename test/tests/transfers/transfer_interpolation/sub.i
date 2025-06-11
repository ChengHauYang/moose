[Mesh]
#  type = GeneratedMesh
#  dim = 2
#  nx = 10
#  ny = 10
  [square_boundary]
    type = PolyLineMeshGenerator
    points = '0.0 0.0 0.0
            1.0 0.0 0.0
            1.0 1.0 0.0
            0.0 1.0 0.0'
    loop = true
  []
  [gen]
    type = XYDelaunayGenerator
    boundary = 'square_boundary'
    desired_area = 0.05
    refine_boundary = false
  []
  [left]
    type = SideSetsFromPointsGenerator
    input = gen
    new_boundary = 'left bottom top right'
    points = '0 0.21 0
              0.21 0 0
              0.21 1 0
              1 0.21 0'
  []
[]

[Variables]
  [./u]
  [../]
[]

[AuxVariables]
  [./nearest_node]
  [../]
  [./mesh_function]
  [../]
  [./user_object]
    order = CONSTANT
    family = MONOMIAL
  [../]
  [./interpolation]
  [../]
[]

[Kernels]
  [./cd]
    type = CoefDiffusion
    variable = u
    coef = 0.1
  [../]
  [./td]
    type = TimeDerivative
    variable = u
  [../]
[]

[BCs]
  [./left]
    type = DirichletBC
    variable = u
    boundary = left
    value = 0
  [../]
  [./right]
    type = DirichletBC
    variable = u
    boundary = right
    value = 1
  [../]
[]

[Executioner]
  type = Transient
  dt = 0.01
  nl_rel_tol = 1e-10

  solve_type = 'PJFNK'

  petsc_options_iname = '-pc_type -pc_hypre_type'
  petsc_options_value = 'hypre boomeramg'
[]

[Outputs]
  exodus = true
[]
