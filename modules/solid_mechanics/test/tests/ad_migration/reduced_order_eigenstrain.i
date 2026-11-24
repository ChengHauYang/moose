# A nonlinear field controls an eigenstrain that is reduced to the strain order before
# entering the displacement residual. The test specification runs both reduction branches.
[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 2
  ny = 2
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Variables]
  [disp_x]
    [InitialCondition]
      type = FunctionIC
      function = '0.1*x + 0.05*x*y'
    []
  []
  [disp_y]
    [InitialCondition]
      type = FunctionIC
      function = '0.2*y + 0.03*x*y'
    []
  []
  [c]
    [InitialCondition]
      type = FunctionIC
      function = '0.4 + 0.1*x + 0.2*y'
    []
  []
[]

[Physics/SolidMechanics/QuasiStatic/all]
  strain = SMALL
  add_variables = false
  use_automatic_differentiation = true
[]

[Kernels]
  [c]
    type = ADReaction
    variable = c
  []
[]

[Materials]
  [prefactor]
    type = ADParsedMaterial
    property_name = eigenstrain_prefactor
    expression = 'c*c + 0.5*c'
    coupled_variables = c
  []
  [source_eigenstrain]
    type = ADComputeEigenstrain
    eigen_base = '0.3 0.1 0 0 0 0.05'
    prefactor = eigenstrain_prefactor
    eigenstrain_name = source_eigenstrain
  []
  [reduced_eigenstrain]
    type = ADComputeReducedOrderEigenstrain
    input_eigenstrain_names = source_eigenstrain
    eigenstrain_name = reduced_eigenstrain
  []
  [elasticity]
    type = ADComputeIsotropicElasticityTensor
    youngs_modulus = 10
    poissons_ratio = 0.25
  []
  [stress]
    type = ADComputeLinearElasticStress
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
[]

[Outputs]
  checkpoint = false
[]
