# Verify KokkosQuantityToNEML2 bulk gathering: a rank-two Kokkos material property is
# gathered into the NEML2 input tensor with correct batch row destinations.
#
# Setup: 2x1 mesh (2 elements, block 0).
# Each tensor component is (3*i+j+1) * 1e-3 * (1+x+2*y), so rows vary by element and QP.
# R2WeightedSumModel contracts all components with weights 1..9. Since sum(k^2)=285,
# the expected pointwise result is 0.285*(1+x+2*y), with domain average 0.7125.
# The averaged absolute pointwise error detects row permutations that an average alone misses.
#
# NEML2Action creates the Kokkos gatherer and batch-index generator. The contraction is
# retrieved as a MOOSE Real material property and averaged over all elements.
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
    values = '1e-3 2e-3 3e-3  4e-3 5e-3 6e-3  7e-3 8e-3 9e-3'
    x_slope = 1
    y_slope = 2
  []
  [gather_error]
    type = ParsedMaterial
    property_name = gather_error
    material_property_names = t
    extra_symbols = 'x y'
    expression = 'abs(t - 0.285*(1+x+2*y))'
  []
[]

[NEML2]
  input = models/kokkos_quantity_to_neml2.i
  load = models/kokkos_r2_scale.py
  eager = true
  device = cpu
  input_types = MATERIAL
  inputs = F
  [weighted]
    model = weighted_model
    block = 0
    input_backend = kokkos
  []
[]

[Postprocessors]
  [t_avg]
    type = ElementAverageMaterialProperty
    mat_prop = t
    execute_on = timestep_end
  []
  [gather_error]
    type = ElementAverageMaterialProperty
    mat_prop = gather_error
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
