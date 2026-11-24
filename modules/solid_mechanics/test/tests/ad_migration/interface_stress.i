!include mechanics_base.i

# Assemble the interface stress as an additional contribution to the elastic residual.
# Both order parameters have nonzero gradients and distinct stress/range coefficients.
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
  [interface]
    type = ADComputeInterfaceStress
    v = 'eta1 eta2'
    stress = '1 2'
    op_range = '0.7 1.3'
    planar_stress_name = interface_stress
  []
[]
