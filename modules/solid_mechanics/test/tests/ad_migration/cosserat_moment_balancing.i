[Mesh]
  type = GeneratedMesh
  dim = 3
[]

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
  Cosserat_rotations = 'wc_x wc_y wc_z'
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
  [wc_x]
  []
  [wc_y]
  []
  [wc_z]
  []
[]

[ICs]
  [disp_x]
    type = FunctionIC
    variable = disp_x
    function = 'x*y + y*z'
  []
  [disp_y]
    type = FunctionIC
    variable = disp_y
    function = 'y*z + z*x'
  []
  [disp_z]
    type = FunctionIC
    variable = disp_z
    function = 'z*x + x*y'
  []
  [wc_x]
    type = FunctionIC
    variable = wc_x
    function = 'x*y + z'
  []
  [wc_y]
    type = FunctionIC
    variable = wc_y
    function = 'y*z + x'
  []
  [wc_z]
    type = FunctionIC
    variable = wc_z
    function = 'z*x + y'
  []
[]

[Kernels]
  [disp_x]
    type = ADStressDivergenceTensors
    variable = disp_x
    component = 0
  []
  [disp_y]
    type = ADStressDivergenceTensors
    variable = disp_y
    component = 1
  []
  [disp_z]
    type = ADStressDivergenceTensors
    variable = disp_z
    component = 2
  []
  [wc_x]
    type = ADMomentBalancing
    variable = wc_x
    component = 0
  []
  [wc_y]
    type = ADMomentBalancing
    variable = wc_y
    component = 1
  []
  [wc_z]
    type = ADMomentBalancing
    variable = wc_z
    component = 2
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeCosseratElasticityTensor
    B_ijkl = 0.5
    E_ijkl = '1 2 1.3333'
    fill_method = general_isotropic
  []
  [strain]
    type = ADComputeCosseratSmallStrain
  []
  [stress]
    type = ADComputeCosseratLinearElasticStress
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
  solve_type = Newton
  num_steps = 1
[]
