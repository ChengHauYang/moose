!include mechanics_base.i

# The stress gradients and both energy properties contribute order-parameter derivatives.
[Kernels]
  [interface_x]
    type = ADStressDivergenceTensors
    variable = disp_x
    component = 0
    base_name = interface
  []
  [interface_y]
    type = ADStressDivergenceTensors
    variable = disp_y
    component = 1
    base_name = interface
  []
  [interface_z]
    type = ADStressDivergenceTensors
    variable = disp_z
    component = 2
    base_name = interface
  []
[]

[Materials]
  [barrier]
    type = ADParsedMaterial
    property_name = g
    coupled_variables = 'eta1 eta2'
    expression = 'eta1^2 + eta2^2'
  []
  [gradient_energy]
    type = ADParsedMaterial
    property_name = kappa_op
    coupled_variables = 'eta1 eta2'
    expression = '1 + eta1*eta2'
  []
  [interface]
    type = ADComputeSurfaceTensionKKS
    v = eta1
    w = 2
    planar_stress_name = interface_stress
  []
[]
