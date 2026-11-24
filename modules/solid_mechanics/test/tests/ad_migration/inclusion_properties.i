# Compare every tensor component and energy inside and outside an elliptical inclusion.
[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 4
  ny = 4
[]

[Problem]
  solve = false
[]

[Variables]
  [dummy]
  []
[]

[GlobalParams]
  # Unequal axes and misfit strains avoid a circular, isotropic special case.
  a = 0.6
  b = 0.4
  lambda = 1
  mu = 1
  misfit_strains = '0.005 0.002'
[]

[Materials]
  [reference]
    type = InclusionProperties
    stress_name = stress_reference
    strain_name = strain_reference
    energy_name = energy_reference
    outputs = all
  []
  [ad]
    type = ADInclusionProperties
    stress_name = stress_ad
    strain_name = strain_ad
    energy_name = energy_ad
    outputs = all
  []
[]

[Postprocessors]
  [stress_00_error]
    type = ElementL2Difference
    variable = stress_ad_00
    other_variable = stress_reference_00
  []
  [stress_01_error]
    type = ElementL2Difference
    variable = stress_ad_01
    other_variable = stress_reference_01
  []
  [stress_02_error]
    type = ElementL2Difference
    variable = stress_ad_02
    other_variable = stress_reference_02
  []
  [stress_10_error]
    type = ElementL2Difference
    variable = stress_ad_10
    other_variable = stress_reference_10
  []
  [stress_11_error]
    type = ElementL2Difference
    variable = stress_ad_11
    other_variable = stress_reference_11
  []
  [stress_12_error]
    type = ElementL2Difference
    variable = stress_ad_12
    other_variable = stress_reference_12
  []
  [stress_20_error]
    type = ElementL2Difference
    variable = stress_ad_20
    other_variable = stress_reference_20
  []
  [stress_21_error]
    type = ElementL2Difference
    variable = stress_ad_21
    other_variable = stress_reference_21
  []
  [stress_22_error]
    type = ElementL2Difference
    variable = stress_ad_22
    other_variable = stress_reference_22
  []
  [strain_00_error]
    type = ElementL2Difference
    variable = strain_ad_00
    other_variable = strain_reference_00
  []
  [strain_01_error]
    type = ElementL2Difference
    variable = strain_ad_01
    other_variable = strain_reference_01
  []
  [strain_02_error]
    type = ElementL2Difference
    variable = strain_ad_02
    other_variable = strain_reference_02
  []
  [strain_10_error]
    type = ElementL2Difference
    variable = strain_ad_10
    other_variable = strain_reference_10
  []
  [strain_11_error]
    type = ElementL2Difference
    variable = strain_ad_11
    other_variable = strain_reference_11
  []
  [strain_12_error]
    type = ElementL2Difference
    variable = strain_ad_12
    other_variable = strain_reference_12
  []
  [strain_20_error]
    type = ElementL2Difference
    variable = strain_ad_20
    other_variable = strain_reference_20
  []
  [strain_21_error]
    type = ElementL2Difference
    variable = strain_ad_21
    other_variable = strain_reference_21
  []
  [strain_22_error]
    type = ElementL2Difference
    variable = strain_ad_22
    other_variable = strain_reference_22
  []
  [energy_error]
    type = ElementL2Difference
    variable = energy_ad
    other_variable = energy_reference
  []
[]

[Executioner]
  type = Steady
[]

[Outputs]
  csv = true
  execute_on = TIMESTEP_END
[]
