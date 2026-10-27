# Verify the NEML2 Kokkos output backend maps a scalar (Real) model output, matching the host
# NEML2ToMOOSERealMaterialProperty retriever.
#
# R2TraceModel emits the scalar t = trace(F) = F_xx + F_yy + F_zz. The rank-two input F varies by
# element and quadrature point through functions of (x, y), so the scalar output is genuinely batched
# across the NEML2 batch dimension rather than broadcast. With the functions below,
#   F_ij = base_ij * (1 + x + 2*y),  base row-major = 1e-3..9e-3,
# so trace(F) = (1e-3 + 5e-3 + 9e-3) * (1 + x + 2*y) = 15e-3 * (1 + x + 2*y). The average captures the
# mean batch value and the maximum the extreme quadrature-point value, so a wrong batch row or a
# dropped component shifts at least one.
#
# F is a host material gathered by the default (moose) NEML2 input backend, matching the working
# kokkos_interface_retriever pattern; only the OUTPUT backend is under test here. The host backend is
# the oracle: the test spec runs this input with output_backend=moose to generate the gold, then with
# output_backend=kokkos (CPU and CUDA) against the same gold, so host and Kokkos scalar retrieval are
# pinned to the same values. The host variant also swaps the two Kokkos element postprocessors for
# their host counterparts via cli_args.

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

[Functions]
  # F_ij = base_ij * (1 + x + 2*y), base row-major 1e-3..9e-3. The common affine factor makes F vary
  # per quadrature point so the NEML2 batch is nontrivial.
  [f11] type = ParsedFunction expression = '1e-3*(1+x+2*y)' []
  [f12] type = ParsedFunction expression = '2e-3*(1+x+2*y)' []
  [f13] type = ParsedFunction expression = '3e-3*(1+x+2*y)' []
  [f21] type = ParsedFunction expression = '4e-3*(1+x+2*y)' []
  [f22] type = ParsedFunction expression = '5e-3*(1+x+2*y)' []
  [f23] type = ParsedFunction expression = '6e-3*(1+x+2*y)' []
  [f31] type = ParsedFunction expression = '7e-3*(1+x+2*y)' []
  [f32] type = ParsedFunction expression = '8e-3*(1+x+2*y)' []
  [f33] type = ParsedFunction expression = '9e-3*(1+x+2*y)' []
[]

[Materials]
  [F]
    type = GenericFunctionRankTwoTensor
    block = 0
    tensor_name = F
    tensor_functions = 'f11 f12 f13 f21 f22 f23 f31 f32 f33'
  []
[]

[NEML2]
  input = models/kokkos_quantity_to_neml2.i
  load = models/kokkos_r2_scale.py
  eager = true
  device = cpu
  input_types = MATERIAL
  inputs = F
  [trace]
    model = trace_model
    block = 0
    output_backend = kokkos
  []
[]

[Postprocessors]
  [t_avg]
    type = KokkosElementAverageMaterialProperty
    mat_prop = t
    execute_on = timestep_end
  []
  [t_max]
    type = KokkosElementExtremeMaterialProperty
    mat_prop = t
    value_type = max
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
