!include mechanics_base.i

# Two independent nonlinear variables drive distinct AD extra-stress producers.
[Materials]
  [prefactor]
    type = ADParsedMaterial
    property_name = extra_stress_prefactor
    coupled_variables = eta1
    expression = '1 + eta1^2'
  []
  [gas_constants]
    type = ADGenericConstantMaterial
    prop_names = 'b Va T'
    prop_values = '0.1 2 1'
  []
  [constant_extra_stress]
    type = ADComputeExtraStressConstant
    base_name = constant
    prefactor = extra_stress_prefactor
    extra_stress_tensor = '1 2 3 0.4 0.5 0.6'
  []
  [gas_extra_stress]
    type = ADComputeExtraStressVDWGas
    base_name = gas
    b = b
    Va = Va
    T = T
    cg = eta2
    nondim_factor = 10000
  []
  [stress]
    ad_extra_stress_names = 'constant_extra_stress gas_extra_stress'
  []
[]
