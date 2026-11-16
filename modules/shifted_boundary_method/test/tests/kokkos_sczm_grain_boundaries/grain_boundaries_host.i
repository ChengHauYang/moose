# Host reference input for 2D plane-strain finite-deformation crystal plasticity
# across five polycrystalline grains coupled to shifted cohesive zones on the seven
# grain boundaries of ../grain_boundary_case/grain_boundary_only.msh.
#
# Counterpart: grain_boundaries_kokkos.i replaces the host total-Lagrangian bulk
# assembly, the host SCZM interface kernel and traction material, the interface
# NEML2 input/output backends, the Dirichlet BCs, and the postprocessors with
# their Kokkos/Torch equivalents.

ny = 6
nx = '${fparse ny * 3 / 2}'
DT = 0.25
NUM_STEPS = 4

interfaces = 'grain1_grain2 grain1_grain4 grain1_grain5 grain2_grain4 grain3_grain4 grain3_grain5 grain4_grain5'

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [boundary_mesh]
    type = FileMeshGenerator
    file = '../grain_boundary_case/grain_boundary_only.msh'
    save_with_name = boundary_mesh
  []
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    xmin = -0.6
    xmax = 0.6
    ymin = -0.4
    ymax = 0.4
    nx = ${nx}
    ny = ${ny}
    subdomain_ids = 10
  []
  [grain_gen]
    type = SubdomainGrainIDGenerator
    input = gen
    boundary_mesh = boundary_mesh
    lambda = 1
  []
  [break7]
    type = BreakMeshByBlockGenerator
    input = grain_gen
  []
  add_subdomain_ids = '10'
  add_subdomain_names = 'block_10'
  final_generator = 'break7'
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = FINITE
        new_system = true
        add_variables = true
        formulation = TOTAL
        volumetric_locking_correction = false
      []
    []
    [ShiftedCohesiveZone]
      [sczm]
        boundary = ${interfaces}
        sbm_distance_uo = distance
        directional_correction = true
        no_shifted = false
        use_kokkos = false
        stress = pk1_stress
        tangent = pk1_jacobian
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
    input_types = 'MATERIAL'
    inputs = 'deformation_gradient'
    derivatives = 'pk1_stress deformation_gradient pk1_jacobian'
    interface = ${interfaces}
    volume = false
    executor_name = 'neml2_interface'
    manage_state_advance = true
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
[]

[Materials]
  [bulk_stress]
    type = ComputeLagrangianStressCustomPK2
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
    boundary = ${interfaces}
    normal_stiffness = 2e3
    tangent_stiffness = 2e3
  []
[]

[UserObjects]
  [manager]
    type = SBMInterfaceManager
    complete_interface_mesh = boundary_mesh
    execution_order_group = -3
  []
  [distance]
    type = BoundaryShortestDistanceToSurface
    manager = manager
    boundary = ${interfaces}
    execute_on = INITIAL
    execution_order_group = -2
  []
[]

[Functions]
  [displacement_with_time]
    type = ParsedFunction
    expression = '1e-2 * t'
  []
[]

[BCs]
  [anchor_x]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [anchor_y]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [displacement_x_right]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = right
    function = displacement_with_time
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
  [avg_disp_x_grain4]
    type = ElementAverageValue
    variable = disp_x
    block = 4
  []
[]

[Outputs]
  exodus = true
[]
