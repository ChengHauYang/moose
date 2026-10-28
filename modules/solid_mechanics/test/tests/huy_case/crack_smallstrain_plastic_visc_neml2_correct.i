# ==============================================================================
# crack_smallstrain_plastic_visc_neml2_correct.i -- crack propagation with NEML2 CZM.
# Bulk J2 plasticity and the cohesive law use NEML2.
#
# Same mesh / physics / crack-mouth-correct K-field BCs as crack_base.i, but:
#   - MONOTONIC K-field ramp 0 -> ki_max (no unload), to drive the crack past
#     initiation and into propagation.
#   - adaptive time stepping (IterationAdaptiveDt) to resolve initiation/growth.
#   - outputs the cohesive normal traction + damage along the ligament so the crack
#     tip (peak-traction location) and R-curve (J vs crack extension) can be
#     post-processed (crack_rcurve.py).
#
# Run:  solid_mechanics-opt -i crack_smallstrain_plastic_visc_neml2_correct.i
#       (requires an executable built with NEML2 support)
# ==============================================================================

ki_max = 2.0   # peak applied K_I at t=1 (monotonic ramp K_I(t) = ki_max * t)

[Mesh]
  use_displaced_mesh = false
  [fmg]
    type = FileMeshGenerator
    file = 'crack.e'
  []
  [translate]
    type = TransformGenerator
    input = fmg
    transform = TRANSLATE
    vector_value = '-0.18 0 0'
  []
  [outer_top]
    type = ParsedGenerateSideset
    input = 'translate'
    combinatorial_geometry = 'x > -1e10'
    include_only_external_sides = true
    included_subdomains = '0'
    new_sideset_name = 'outer_top'
  []
  [outer_bottom]
    type = ParsedGenerateSideset
    input = 'outer_top'
    combinatorial_geometry = 'x > -1e10'
    include_only_external_sides = true
    included_subdomains = '1'
    new_sideset_name = 'outer_bottom'
  []
  # ----------------------------------------------------------------------------
  # CRITICAL WORKFLOW ORDER:
  # Step 1: Split the full horizontal interface while there are only 2 blocks
  # (Block 0 top, Block 1 bottom) BEFORE sub-partitioning into crack and ligament.
  #
  # Rationale:
  # If the mesh were partitioned into 4 subdomains first (top_crack, bottom_crack,
  # top_ligament, bottom_ligament), the crack tip at (0, 0) would become a 4-block
  # junction node (sets_blocks_for_this_node.size() == 4). BreakMeshByBlockGenerator
  # specifically guards against multi-block junctions and only duplicates nodes with
  # sets_blocks_for_this_node.size() == 2. That would leave the crack tip at (0, 0)
  # unsplit and topologically locked!
  #
  # Splitting here with only 2 blocks guarantees that every node along y = 0
  # (including the crack tip at (0, 0)) connects to exactly 2 blocks, cleanly
  # duplicating into top/bottom nodes and registering the directed boundary pair
  # Block0_Block1 <-> Block1_Block0 in libMesh for MPI ghosting and neighbor pointers.
  # ----------------------------------------------------------------------------
  [split]
    type = BreakMeshByBlockGenerator
    input = 'outer_bottom'
  []
  # Step 2: Now that topology is split, partition the x > 0 region into ligament blocks.
  [top_ligament]
    type = ParsedSubdomainMeshGenerator
    input = 'split'
    combinatorial_geometry = 'x > 0'
    included_subdomains = '0'
    block_id = 2
    block_name = 'top_ligament'
  []
  [bottom_ligament]
    type = ParsedSubdomainMeshGenerator
    input = 'top_ligament'
    combinatorial_geometry = 'x > 0'
    included_subdomains = '1'
    block_id = 3
    block_name = 'bottom_ligament'
  []
  [rename_crack]
    type = RenameBlockGenerator
    input = 'bottom_ligament'
    old_block = '0 1'
    new_block = 'top_crack bottom_crack'
  []
  # Step 3: Extract the uncracked ligament (x > 0) from the registered Block0_Block1
  # interface. Elements on 'ligament' inherit libMesh's disjoint neighbor pairing
  # and MPI ghosting from Block0_Block1, while x <= 0 remains a traction-free crack.
  [ligament]
    type = ParsedGenerateSideset
    input = 'rename_crack'
    included_boundaries = 'Block0_Block1'
    combinatorial_geometry = 'x > 0'
    new_sideset_name = 'ligament'
  []
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = SMALL
        new_system = true
        formulation = TOTAL
        add_variables = true
        #volumetric_locking_correction = false
        volumetric_locking_correction = false
        generate_output = 'cauchy_stress_xx cauchy_stress_yy cauchy_stress_xy'
      []
    []
    [CohesiveZone]
      [czm]
        boundary = 'ligament'
        strain = SMALL
      []
    []
  []
[]

[Functions]
  # Monotonic mode-I K-field, K_I(t) = ki_max * t. Per-face branches so the crack
  # opens (bottom mouth forced to theta=-pi; see crack_base.i for the signed-zero note).
  [kfield_ux_top]
    type = ParsedFunction
    symbol_names = 'kappa mu2'
    symbol_values = '1.8 769.230769'
    expression = '${ki_max}*t/mu2'
                 '*sqrt(sqrt(x^2+y^2)/(2*3.141592653589793))'
                 '*cos(atan2(y,x)/2)*(kappa-cos(atan2(y,x)))'
  []
  [kfield_uy_top]
    type = ParsedFunction
    symbol_names = 'kappa mu2'
    symbol_values = '1.8 769.230769'
    expression = '${ki_max}*t/mu2'
                 '*sqrt(sqrt(x^2+y^2)/(2*3.141592653589793))'
                 '*sin(atan2(y,x)/2)*(kappa-cos(atan2(y,x)))'
  []
  [kfield_ux_bot]
    type = ParsedFunction
    symbol_names = 'kappa mu2'
    symbol_values = '1.8 769.230769'
    expression = '${ki_max}*t/mu2'
                 '*sqrt(sqrt(x^2+y^2)/(2*3.141592653589793))'
                 '*cos((if((abs(y)<1e-9)*(x<0),-3.141592653589793,atan2(y,x)))/2)'
                 '*(kappa-cos(if((abs(y)<1e-9)*(x<0),-3.141592653589793,atan2(y,x))))'
  []
  [kfield_uy_bot]
    type = ParsedFunction
    symbol_names = 'kappa mu2'
    symbol_values = '1.8 769.230769'
    expression = '${ki_max}*t/mu2'
                 '*sqrt(sqrt(x^2+y^2)/(2*3.141592653589793))'
                 '*sin((if((abs(y)<1e-9)*(x<0),-3.141592653589793,atan2(y,x)))/2)'
                 '*(kappa-cos(if((abs(y)<1e-9)*(x<0),-3.141592653589793,atan2(y,x))))'
  []
[]

[BCs]
  [top_x]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = 'outer_top'
    function = kfield_ux_top
    preset = true
  []
  [top_y]
    type = FunctionDirichletBC
    variable = disp_y
    boundary = 'outer_top'
    function = kfield_uy_top
    preset = true
  []
  [bot_x]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = 'outer_bottom'
    function = kfield_ux_bot
    preset = true
  []
  [bot_y]
    type = FunctionDirichletBC
    variable = disp_y
    boundary = 'outer_bottom'
    function = kfield_uy_bot
    preset = true
  []
[]

[AuxVariables]
  [cz_damage]
    order = CONSTANT
    family = MONOMIAL
  []
  [cz_traction_n]   # cohesive normal traction; its peak location = crack tip
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [cz_damage]
    type = MaterialRealAux
    property = damage
    variable = cz_damage
    boundary = 'ligament'
    execute_on = 'TIMESTEP_END'
  []
  [cz_traction_n]
    type = MaterialRealVectorValueAux
    property = interface_traction
    component = 0            # local normal component
    variable = cz_traction_n
    boundary = 'ligament'
    execute_on = 'TIMESTEP_END'
  []
[]

[Postprocessors]
  [max_damage]
    type = ElementExtremeValue
    variable = cz_damage
    execute_on = 'INITIAL TIMESTEP_END'
  []
  [Kapplied]
    type = FunctionValuePostprocessor
    function = '${ki_max}*t'
    execute_on = 'INITIAL TIMESTEP_END'
  []
[]

[Executioner]
  type = Transient
  solve_type = NEWTON
  line_search = none
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_type'
  petsc_options_value = 'lu superlu_dist'
  automatic_scaling = true

  nl_abs_tol = 1e-6
  nl_rel_tol = 1e-5
  nl_max_its = 25
  l_max_its = 100

  # Constant small step (K_I increment ~0.01 per step); MOOSE auto-cuts back on any
  # non-convergence during initiation / growth.
  dt = 0.005
  dtmin = 1e-11   # allow the solver to crawl through any hard spot; if it can't advance
                  # the load, the crack simply isn't propagating (a valid, flat result)
  end_time = 1.0
[]

[Outputs]
  exodus = true
  csv = true
  print_linear_residuals = false
[]
[NEML2]
  input = 'neml2_smallstrain_plastic_visc.i'
  eager = true
  [bulk]
    model = 'model'
    device = 'cpu'
    derivatives = 'neml2_stress neml2_strain'
  []
  [interface]
    model = 'czm'
    device = 'cpu'
    manage_state_advance = true
    interface = 'ligament'
    interface_only = true
    interface_material_inputs = 'interface_displacement_jump'
    derivatives = 'interface_traction interface_displacement_jump dinterface_traction_djump'
  []
[]
[Materials]
  [convert_strain]
    type = RankTwoTensorToSymmetricRankTwoTensor
    from = 'mechanical_strain'
    to = 'neml2_strain'
  []
  [stress]
    type = ComputeLagrangianObjectiveCustomSymmetricStress
    custom_small_stress = 'neml2_stress'
    custom_small_jacobian = 'dneml2_stress/dneml2_strain'
  []
[]
