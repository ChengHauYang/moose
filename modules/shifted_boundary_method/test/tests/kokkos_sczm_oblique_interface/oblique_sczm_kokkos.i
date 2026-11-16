# Kokkos counterpart of oblique_sczm_host.i.
# Replaces the host total-Lagrangian bulk assembly, host SCZM interface kernel and
# traction material, host interface NEML2 input/output backends, Dirichlet BCs,
# postprocessors, and AuxKernel with TorchDeformationGradient,
# NEML2ToKokkosFullRankTwoMaterialProperty, NEML2ToKokkosFullRankFourMaterialProperty,
# KokkosComputeLagrangianStressCustomPK2, KokkosTotalLagrangianStressDivergence,
# KokkosComputeDeformationGradient, KokkosInterfaceQuantityToNEML2,
# KokkosSCZMInterfaceKernelSmallStrain, KokkosPureElasticTractionSeparation,
# KokkosDirichletBC, Kokkos postprocessors, and KokkosMaterialRealAux.
# Stepped/oblique mesh from SubdomainInterceptedGenerator, NEML2 constitutive models,
# solver options, time stepping, and outputs are unchanged.

N = 4
DT = 5e-3
NUM_STEPS = 2

theta = 60
theta_rad = '${fparse theta * pi / 180}'

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 3
    nx = '${fparse 2 * N}'
    ny = ${N}
    nz = ${N}
    xmax = 2
  []
  [intercepted]
    type = SubdomainInterceptedGenerator
    input = gen
    subdomain_id_inside = 0
    subdomain_id_outside = 1
    is_domain_inside_surface = true
    signed_dist_function = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = intercepted
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [disp_z]
  []
[]

[NEML2]
  eager = true
  [bulk]
    input = '../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i'
    model = 'model'
    device = 'cpu'
    executor_name = 'neml2'
    input_kernels = 'deformation_gradient'
    auto_output = true
    output_backend = kokkos
    output_variables = 'slip_hardening'
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
  [interface]
    input = 'exact_kinematics_pk1_neml2.i'
    model = 'interface_model'
    device = 'cpu'
    executor_name = 'neml2_interface'
    input_backend = kokkos
    input_types = 'MATERIAL'
    inputs = 'deformation_gradient'
    derivatives = 'pk1_stress deformation_gradient pk1_jacobian'
    output_backend = kokkos
    output_variables = 'pk1_stress plastic_deformation_gradient'
    interface = 'Block0_Block1'
    volume = false
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
[]

[UserObjects]
  [assembly]
    type = TorchAssembly
  []
  [fe]
    type = TorchFEInterpolation
    assembly = assembly
  []
  [deformation_gradient]
    type = TorchDeformationGradient
    assembly = assembly
    fe = fe
    to_neml2 = deformation_gradient
    displacements = 'disp_x disp_y disp_z'
    stabilize_strain = false
  []
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = 'Block0_Block1'
    execute_on = INITIAL
    execution_order_group = -2
  []
[]

[Kernels]
  [stress_x]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_x
    component = 0
    displacements = 'disp_x disp_y disp_z'
    stabilize_strain = false
  []
  [stress_y]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_y
    component = 1
    displacements = 'disp_x disp_y disp_z'
    stabilize_strain = false
  []
  [stress_z]
    type = KokkosTotalLagrangianStressDivergence
    variable = disp_z
    component = 2
    displacements = 'disp_x disp_y disp_z'
    stabilize_strain = false
  []
[]

[Physics]
  [SolidMechanics]
    [ShiftedCohesiveZone]
      [sczm]
        boundary = 'Block0_Block1'
        displacements = 'disp_x disp_y disp_z'
        sbm_distance_uo = distance
        use_kokkos = true
        directional_correction = true
        no_shifted = false
        stress = pk1_stress
        tangent = pk1_jacobian
        tangent_definition = pk1_wrt_deformation_gradient
      []
    []
  []
[]

[Materials]
  [interface_deformation_gradient]
    type = KokkosComputeDeformationGradient
    displacements = 'disp_x disp_y disp_z'
    deformation_gradient = deformation_gradient
  []
  [stress]
    type = NEML2ToKokkosFullRankTwoMaterialProperty
    neml2_executor = neml2
    from_neml2 = neml2_stress
    to_moose = neml2_stress
  []
  [tangent]
    type = NEML2ToKokkosFullRankFourMaterialProperty
    neml2_executor = neml2
    from_neml2 = neml2_stress
    neml2_input_derivative = deformation_gradient
    to_moose = dneml2_stress_ddeformation_gradient
  []
  [pk1]
    type = KokkosComputeLagrangianStressCustomPK2
    displacements = 'disp_x disp_y disp_z'
    custom_pk2_stress = neml2_stress
    custom_pk2_jacobian = dneml2_stress_ddeformation_gradient
    stabilize_strain = false
  []
  [initial_orientation]
    type = GenericConstantVectorMaterial
    prop_names = 'orientation'
    prop_values = '-0.54412095 -0.34931944 0.12600655'
  []
  [initial_plastic_defgrad]
    type = GenericConstantRankTwoTensor
    tensor_name = initial_plastic_defgrad
    tensor_values = '1 1 1'
  []
  [local_traction]
    type = KokkosPureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
  [loading]
    type = ParsedFunction
    expression = t
  []
[]

[BCs]
  [xfix]
    type = KokkosDirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [yfix]
    type = KokkosDirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [zfix]
    type = KokkosDirichletBC
    variable = disp_z
    boundary = back
    value = 0
  []
  [xdisp]
    type = KokkosDirichletBC
    variable = disp_x
    boundary = right
    preset = false
    value = 0
  []
[]

[Controls]
  [loading]
    type = RealFunctionControl
    parameter = 'BCs/xdisp/value'
    function = loading
    execute_on = 'INITIAL TIMESTEP_BEGIN'
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
  solve_type = NEWTON
  line_search = none
  petsc_options_iname = '-pc_type -pc_hypre_type -ksp_type'
  petsc_options_value = 'hypre boomeramg    gmres'
  automatic_scaling = true
  dt = ${DT}
  num_steps = ${NUM_STEPS}
  residual_and_jacobian_together = true
  # Regold only from a host (CPU) solve at these tolerances, checked against a direct (LU) solve.
  # Tight nonlinear tolerances make the result independent of where Newton stops. A tight l_tol
  # removes the differences that the host and device BoomerAMG defaults (CPU vs GPU matrix types)
  # would otherwise leave in each Newton update, so all variants match the same gold.
  nl_rel_tol = 1e-12
  nl_abs_tol = 1e-12
  l_tol = 1e-10
[]

[Postprocessors]
  [max_slip_hardening]
    type = KokkosElementExtremeMaterialProperty
    mat_prop = slip_hardening
    value_type = max
  []
  [avg_slip_hardening]
    type = KokkosElementAverageMaterialProperty
    mat_prop = slip_hardening
  []
  [avg_disp_x_block1]
    type = KokkosElementAverageValue
    variable = disp_x
    block = 1
  []
[]

[AuxVariables]
  [slip_hardening_elem]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [slip_hardening_elem]
    type = KokkosMaterialRealAux
    variable = slip_hardening_elem
    property = slip_hardening
    execute_on = TIMESTEP_END
  []
[]

[Outputs]
  exodus = true
[]
