# NEML2 v3 models for the PLASTIC crack case: bulk J2 plasticity ('model') +
# bilinear mixed-mode traction-separation law ('czm') in ONE file.
#
# bulk 'model' : total-Lagrangian St.Venant-Kirchhoff J2 with linear isotropic
#   hardening. NEML2 v3 has no macroscopic multiplicative finite-strain J2, so we feed
#   the Green-Lagrange strain as the total strain into the small-strain associative J2
#   return map and read the (energy-conjugate) stress as PK2. E=1000, nu=0.3,
#   yield stress = 1.2, hardening slope H = 100 ((2.2-1.2)/0.01 from Abaqus *Plastic).
#   deformation_gradient (R2) -> PK2 (R2) + dpk2_dF.
# czm 'czm'    : identical bilinear TSL as the elastic case (see param rationale there).
[Models]
  # --- J2 return-map ingredients (small-strain measures on neml2_strain) ---
  [isoharden]
    type = LinearIsotropicHardening
    hardening_modulus = 100
  []
  [elastic_strain]
    type = SR2LinearCombination
    from = 'neml2_strain plastic_strain'
    to = 'elastic_strain'
    weights = '1 -1'
  []
  [elasticity]
    type = LinearIsotropicElasticity
    coefficients = '1000 0.3'
    coefficient_types = 'YOUNGS_MODULUS POISSONS_RATIO'
    strain = 'elastic_strain'
    stress = 'neml2_stress'
  []
  [mandel_stress]
    type = IsotropicMandelStress
    cauchy_stress = 'neml2_stress'
  []
  [vonmises]
    type = SR2Invariant
    invariant_type = 'VONMISES'
    tensor = 'mandel_stress'
    invariant = 'effective_stress'
  []
  [yield_surface]
    type = YieldFunction
    yield_stress = 1.2
    isotropic_hardening = 'isotropic_hardening'
  []
  [flow]
    type = ComposedModel
    models = 'vonmises yield_surface'
  []
  [normality]
    type = Normality
    model = 'flow'
    function = 'yield_function'
    from = 'mandel_stress isotropic_hardening'
    to = 'flow_direction isotropic_hardening_direction'
  []
  [eprate]
    type = AssociativeIsotropicPlasticHardening
  []
  [Eprate]
    type = AssociativePlasticFlow
  []
  [integrate_ep]
    type = ScalarBackwardEulerTimeIntegration
    variable = 'equivalent_plastic_strain'
  []
  [integrate_Ep]
    type = SR2BackwardEulerTimeIntegration
    variable = 'plastic_strain'
  []
  [consistency]
    type = FBComplementarity
    a = 'yield_function'
    a_inequality = 'LE'
    b = 'flow_rate'
  []
  [surface]
    type = ComposedModel
    models = "isoharden elastic_strain elasticity
              mandel_stress vonmises
              yield_surface normality eprate Eprate
              consistency integrate_ep integrate_Ep"
  []
[]

[EquationSystems]
  [eq_sys]
    type = NonlinearSystem
    model = 'surface'
    unknowns = 'plastic_strain equivalent_plastic_strain flow_rate'
    residuals = 'plastic_strain_residual equivalent_plastic_strain_residual complementarity'
  []
[]

[Solvers]
  [newton]
    type = Newton
    linear_solver = 'lu'
  []
  [lu]
    type = DenseLU
  []
[]

[Models]
  [predictor]
    type = ConstantExtrapolationPredictor
    unknowns_SR2 = 'plastic_strain'
    unknowns_Scalar = 'equivalent_plastic_strain flow_rate'
  []
  [return_map]
    type = ImplicitUpdate
    equation_system = 'eq_sys'
    solver = 'newton'
    predictor = 'predictor'
  []
  [model]
    type = ComposedModel
    models = 'return_map elastic_strain elasticity'
    additional_outputs = 'neml2_stress plastic_strain equivalent_plastic_strain'
  []

  # ================= cohesive zone (bilinear mixed-mode TSL) =================
  [decompose]
    type = VecComponents
    from = 'interface_displacement_jump'
    to = 'signed_normal_separation tangential_separation_1 tangential_separation_2'
  []
  [tangential_separation]
    type = ScalarPNorm
    from = 'tangential_separation_1 tangential_separation_2'
    to = 'tangential_separation'
    exponent = 2.0
  []
  [macaulay_n]
    type = MacaulaySplit
    from = 'signed_normal_separation'
    to_positive = 'normal_separation'
    to_negative = 'normal_penetration'
  []
  [mode_mixity]
    type = ModeMixity
  []
  [critical_separation]
    type = CamanhoDavilaCriticalSeparation
    mode_mixity = 'mode_mixity'
    penalty_stiffness = 1e5
    normal_strength = 3.0
    shear_strength = 3.0
  []
  [full_separation]
    type = BenzeggaghKenaneFullSeparation
    mode_mixity = 'mode_mixity'
    critical_separation = 'critical_separation'
    penalty_stiffness = 1e5
    mode_I_fracture_toughness = 0.001
    mode_II_fracture_toughness = 0.001
    shear_strength = 3.0
    eta = 2.0
  []
  [effective_separation]
    type = ScalarPNorm
    from = 'normal_separation tangential_separation'
    to = 'effective_separation'
    exponent = 2.0
  []
  [traction]
    type = BilinearTraction
    traction = 'interface_traction'
    critical_separation = 'critical_separation'
    full_separation = 'full_separation'
    normal_penetration = 'normal_penetration'
    penalty_stiffness = 1e5
    time = 't'
    #viscosity = 0.0001
    viscosity = 0.001
  []
  [czm]
    type = ComposedModel
    models = 'decompose tangential_separation macaulay_n effective_separation traction'
    additional_outputs = 'damage'
  []
[]
