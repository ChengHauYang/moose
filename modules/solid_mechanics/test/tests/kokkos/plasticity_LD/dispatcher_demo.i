N = 3

# Standalone copy of step1_plasticity_cpu_neml2.i wired to the cpp-aoti
# dispatcher instead of the cpp-eager runtime. Everything except the [NEML2]
# block is identical to step 1, so the two runs differ only in how the
# constitutive update is dispatched.
#
# Run run_dispatcher_demo.sh first: it compiles the model below into the
# aoti/model artifact folder that [NEML2]/input points at.

[GlobalParams]
  displacements = 'disp_x disp_y disp_z'
[]

[Mesh]
  [generated]
    type = GeneratedMeshGenerator
    dim = 3
    nx = ${N}
    ny = ${N}
    nz = ${N}
  []
[]

[Physics]
  [SolidMechanics]
    [QuasiStatic]
      [all]
        strain = FINITE
        formulation = TOTAL
        new_system = true
        add_variables = true
        volumetric_locking_correction = true
      []
    []
  []
[]

# eager = false routes the constitutive update through
# neml2::aoti::DispatchedModel with an MPISimpleScheduler. input points at the
# self-describing artifact folder written by neml2-compile instead of the
# source .i. device_batch chunks the leading batch axis so the dispatch is
# visible on this small mesh.
[NEML2]
  eager = false
  input = 'aoti/model'
  output_device = cpu
  [all]
    model = model
    device = cpu
    device_batch = 8
    derivatives = 'neml2_stress deformation_gradient'
    initialize_outputs = 'plastic_deformation_gradient'
    initialize_output_values = 'initial_plastic_defgrad'
  []
[]

[Materials]
  [stress]
    type = ComputeLagrangianStressCustomPK2
    custom_pk2_stress = neml2_stress
    custom_pk2_jacobian = 'dneml2_stress/ddeformation_gradient'
    large_kinematics = true
  []
  [initial_orientation]
    type = GenericConstantRealVectorValue
    vector_name = orientation
    vector_values = '-0.54412095 -0.34931944 0.12600655'
  []
  [initial_plastic_defgrad]
    type = GenericConstantRankTwoTensor
    tensor_name = initial_plastic_defgrad
    tensor_values = '1 1 1'
  []
[]

[BCs]
  [disp_x_left]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0
  []
  [disp_x_right]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = right
    function = t
    preset = false
  []
  [disp_y]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [disp_z]
    type = DirichletBC
    variable = disp_z
    boundary = back
    value = 0
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
  dt = 1e-3
  dtmin = 1e-3
  num_steps = 1
  nl_abs_tol = 1e-10
  residual_and_jacobian_together = true
  automatic_scaling = true
[]

[Postprocessors]
  [ux_right]
    type = PointValue
    variable = disp_x
    point = '1 0.5 0.5'
    execute_on = TIMESTEP_END
  []
  [ux_center]
    type = PointValue
    variable = disp_x
    point = '0.5 0.5 0.5'
    execute_on = TIMESTEP_END
  []
[]

[Outputs]
  exodus = false
  csv = true
[]