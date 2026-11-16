# Host reference input for 3D finite-deformation total-Lagrangian crystal plasticity
# in two bulk blocks coupled to a total-Lagrangian shifted cohesive-zone interface
# (strain = FINITE) on Block0_Block1 with total-mode F-bar volumetric locking
# correction in both the bulk and the interface directional correction (PK1 = U * PK2(B),
# d(PK1)/dU, and d(PK1)/dB from NEML2).
#
# Counterpart: tl_sczm_kokkos.i replaces the host total-Lagrangian F-bar bulk assembly,
# the host total-Lagrangian SCZM interface kernel and traction material, the interface
# NEML2 input/output backends, the Dirichlet BCs, the postprocessors, and the AuxKernel
# with their Kokkos/Torch equivalents.

N = 2
DT = 5e-3
NUM_STEPS = 3

theta = 60
theta_rad = '${fparse theta * pi / 180}'

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 3
    nx = '${fparse 2 * N}'
    ny = ${N}
    nz = ${N}
    xmax = 2
  []
  [right_block]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '1 0 0'
    top_right = '2 1 1'
    block_id = 1
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = right_block
  []
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = FINITE
        new_system = true
        add_variables = true
        formulation = TOTAL
        volumetric_locking_correction = true
        base_name = bulk
        strain_base_name = bulk
      []
    []
    [ShiftedCohesiveZone]
      [sczm]
        boundary = 'Block0_Block1'
        sbm_distance_uo = distance
        directional_correction = true
        no_shifted = false
        use_kokkos = false
        strain = FINITE
        volumetric_locking_correction = true
        stress = pk1_stress
        tangent = pk1_jacobian
        stabilized_tangent = stabilized_pk1_jacobian
        tangent_definition = pk1_wrt_deformation_gradient
      []
    []
  []
[]

[NEML2]
  eager = true
  [bulk]
    input = '../../../../solid_mechanics/test/tests/neml2/crystal_plasticity/exact_kinematics_neml2.i'
    model = 'model'
    device = 'cpu'
    derivatives = 'neml2_stress deformation_gradient'
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
    manage_state_advance = true
  []
  [interface]
    input = 'exact_kinematics_pk1_neml2.i'
    model = 'interface_model'
    device = 'cpu'
    input_types = 'MATERIAL MATERIAL'
    inputs = 'physical_deformation_gradient deformation_gradient'
    derivatives = 'pk1_stress physical_deformation_gradient pk1_jacobian; pk1_stress deformation_gradient stabilized_pk1_jacobian'
    interface = 'Block0_Block1'
    volume = false
    executor_name = 'neml2_interface'
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
[]

[Materials]
  [deformation_gradient]
    type = ComputeDeformationGradient
    stabilize_strain = true
  []
  [bulk_stress]
    type = ComputeLagrangianStressCustomPK2
    base_name = bulk
    custom_pk2_stress = 'neml2_stress'
    custom_pk2_jacobian = 'dneml2_stress/ddeformation_gradient'
  []
  [initial_orientation]
    type = GenericConstantVectorMaterial
    prop_names = 'orientation'
    prop_values = '-0.54412095 -0.34931944 0.12600655'
  []
  [initial_plastic_defgrad]
    type = GenericConstantRankTwoTensor
    tensor_name = 'initial_plastic_defgrad'
    tensor_values = '1 1 1'
  []
  [local_traction]
    type = PureElasticTractionSeparation
    boundary = 'Block0_Block1'
    normal_stiffness = 10
    tangent_stiffness = 8
  []
[]

[UserObjects]
  [distance]
    type = BoundaryShortestDistanceToSurface
    surfaces = true_interface
    boundary = 'Block0_Block1'
    execute_on = INITIAL
    execution_order_group = -2
  []
[]

[Functions]
  [true_interface]
    type = ParsedFunction
    expression = '(x - 1.1) * sin(${theta_rad}) - (y - 0.5) * cos(${theta_rad})'
  []
[]

[BCs]
  [xfix]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [yfix]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [zfix]
    type = DirichletBC
    variable = disp_z
    boundary = back
    value = 0
  []
  [xdisp]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = right
    function = t
    preset = false
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
  nl_rel_tol = 1e-12
  nl_abs_tol = 1e-12
  l_tol = 1e-10
[]

[Postprocessors]
  [max_slip_hardening]
    type = ElementExtremeMaterialProperty
    mat_prop = slip_hardening
    value_type = max
  []
  [avg_slip_hardening]
    type = ElementAverageMaterialProperty
    mat_prop = slip_hardening
  []
  [avg_disp_x_block1]
    type = ElementAverageValue
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
    type = MaterialRealAux
    variable = slip_hardening_elem
    property = slip_hardening
    execute_on = TIMESTEP_END
  []
[]

[Outputs]
  exodus = true
[]
