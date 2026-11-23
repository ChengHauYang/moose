# Use the same heated cantilever with beam variables and equations created by the Physics block.
!include ad_thermal_beam_jac.i

[Variables]
  active = 'T'
[]

[Kernels]
  active = 'T_dt'
[]

[Materials]
  inactive = 'strain stress'
[]

[Physics/SolidMechanics/LineElement/QuasiStatic]
  use_automatic_differentiation = true
  add_variables = true
  displacements = 'disp_x disp_y disp_z'
  rotations = 'rot_x rot_y rot_z'

  [beam]
    block = 0
    area = 0.5
    Iy = 0.01
    Iz = 0.01
    y_orientation = '0.0 1.0 0.0'
    eigenstrain_names = 'thermal'
  []
[]
