!include mechanics_base.i

[Variables]
  [added_mass]
  []
[]

[AuxVariables]
  [velocity]
    initial_condition = 1
  []
  [acceleration]
    initial_condition = 0.5
  []
[]

[Kernels]
  [inertia]
    type = ADInertialForce
    variable = disp_x
    density = scaled_density
    velocity = velocity
    acceleration = acceleration
    # Average-acceleration Newmark parameters.
    beta = 0.25
    gamma = 0.5
    use_displaced_mesh = false
  []
  [added_mass]
    type = ADMaterialPropertyValue
    variable = added_mass
    prop_name = additional_density
  []
[]

[Materials]
  [density]
    type = ADParsedMaterial
    property_name = density
    coupled_variables = eta1
    expression = '2 + eta1^2'
  []
  [stiffness]
    type = ADParsedMaterial
    property_name = effective_stiffness
    coupled_variables = eta2
    expression = '1 + eta2^2'
  []
  [scaling]
    type = ADDensityScaling
    true_density = density
    scaled_density = scaled_density
    # Keep the stable-density branch well above the true density throughout this mesh.
    desired_time_step = 10
  []
[]
