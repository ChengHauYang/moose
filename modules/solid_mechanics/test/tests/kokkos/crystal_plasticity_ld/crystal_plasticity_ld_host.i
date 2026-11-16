# Host reference input for 3D total-Lagrangian finite-strain crystal plasticity
# with total-mode F-bar volumetric locking correction, evaluated with NEML2
# (../../neml2/crystal_plasticity/exact_kinematics_neml2.i) on a 2x2x2 cube under
# uniaxial displacement control u_x(1, y, z) = t.
#
# Counterpart: crystal_plasticity_ld_kokkos.i replaces the host total-Lagrangian
# F-bar kinematics, PK2->PK1 stress bridge, stress-divergence kernel, and Dirichlet
# BCs with TorchDeformationGradient, KokkosComputeFbarAverage,
# NEML2ToKokkosFullRankTwoMaterialProperty, NEML2ToKokkosFullRankFourMaterialProperty,
# KokkosComputeLagrangianStressCustomPK2, KokkosTotalLagrangianStressDivergence, and
# KokkosDirichletBC driven by RealFunctionControl.

N = 2

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

[NEML2]
  eager = true
  input = '../../neml2/crystal_plasticity/exact_kinematics_neml2.i'
  output_device = cpu
  [all]
    model = model
    device = cpu
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
  num_steps = 5
  # Regold only from a host (CPU) solve at these tolerances, checked against a direct (LU) solve.
  # Tight nonlinear tolerances make the result independent of where Newton stops. A tight l_tol
  # removes the differences that the host and device BoomerAMG defaults (CPU vs GPU matrix types)
  # would otherwise leave in each Newton update, so all variants match the same gold.
  nl_rel_tol = 1e-12
  nl_abs_tol = 1e-12
  l_tol = 1e-10
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
  exodus = true
[]
