# Verify KokkosQuantityToNEML2 bulk gathering: a rank-two Kokkos material property is
# gathered into the NEML2 input tensor with correct batch row destinations.
#
# Setup: 2x1 mesh (2 elements, block 0).
# KokkosConstantRankTwoMaterial supplies F = [[1,2,3],[4,5,6],[7,8,9]] * 1e-3 at every QP.
# NEML2 R2TraceModel computes t = trace(F) = F_xx + F_yy + F_zz.
# Expected: t = (1 + 5 + 9) * 1e-3 = 15e-3 = 0.015.
#
# The gatherer (group 0) is declared manually; NEML2Action is not used because
# input_backend = kokkos is a Phase 6 addition.  The trace is retrieved as a
# MOOSE Real material property and averaged over all elements.
#
# A simple diffusion problem with a zero forcing term forces an element-loop
# material evaluation that allows ElementAverageMaterialProperty to read 't'.
[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 2
    ny = 1
  []
[]

[Variables]
  [u]
  []
[]

[Kernels]
  [diff]
    type = MatDiffusion
    variable = u
    diffusivity = t
  []
[]

[BCs]
  [left]
    type = DirichletBC
    variable = u
    boundary = left
    value = 0
  []
  [right]
    type = DirichletBC
    variable = u
    boundary = right
    value = 1
  []
[]

[Materials]
  [F_kokkos]
    type = KokkosConstantRankTwoMaterial
    block = 0
    prop_name = F
    # Row-major 3x3: F[i][j] = (3i+j+1) * 1e-3  → trace = (1+5+9)*1e-3 = 0.015
    values = '1e-3 2e-3 3e-3  4e-3 5e-3 6e-3  7e-3 8e-3 9e-3'
  []
  [t_retriever]
    type = NEML2ToMOOSERealMaterialProperty
    to_moose = t
    from_neml2 = t
    neml2_executor = trace_executor
  []
[]

[UserObjects]
  [batch_index_generator]
    type = NEML2BatchIndexGenerator
    block = 0
    execute_on = 'initial linear nonlinear'
  []
  [F_gatherer]
    type = KokkosQuantityToNEML2
    block = 0
    from_moose = F
    to_neml2 = F
    batch_index_generator = batch_index_generator
    execute_on = 'initial linear nonlinear'
  []
  [trace_executor]
    type = NEML2ModelExecutor
    eager = true
    input = models/kokkos_quantity_to_neml2.i
    load = models/kokkos_r2_scale.py
    model = trace_model
    device = cpu
    batch_index_generator = batch_index_generator
    gatherers = 'F_gatherer'
    execute_on = 'initial linear nonlinear'
  []
[]

[Postprocessors]
  [t_avg]
    type = ElementAverageMaterialProperty
    mat_prop = t
    execute_on = timestep_end
  []
[]

[Executioner]
  type = Transient
  num_steps = 1
[]

[Outputs]
  csv = true
[]
