# Jacobian check for an interface_only NEML2 model consumed on BOTH sides of a split interface.
#
# This mirrors the volume (block) boundary_restriction_jacobian test (interface_material.i) but with
# interface_only = true, and uses MaterialPropertySource, which reads s and ds/du from both the
# primary (getMaterialProperty) and neighbor (getNeighborMaterialProperty) sides and assembles a
# hand-coded Jacobian over the element-element, element-neighbor, neighbor-element, and
# neighbor-neighbor couplings. The NEML2 input is the solution variable u (input_types = VARIABLE),
# so ds/du is a genuine derivative: side A's s depends on u on A, side B's s depends on u on B, and
# the two sides are independent (dsource_du_neighbor = 0). The two-sided output fix is what makes the
# neighbor-side s and ds/du nonzero, so the neighbor couplings exercise real values here.

[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 10
    ny = 3
  []
  [A]
    type = SubdomainBoundingBoxGenerator
    input = 'gmg'
    block_id = 0
    block_name = 'A'
    bottom_left = '0 0 0'
    top_right = '0.5 1 0'
  []
  [B]
    type = SubdomainBoundingBoxGenerator
    input = 'A'
    block_id = 1
    block_name = 'B'
    bottom_left = '0.5 0 0'
    top_right = '1 1 0'
  []
  [break]
    type = BreakMeshByBlockGenerator
    input = 'B'
  []
[]

[Variables]
  [u]
  []
[]

[Kernels]
  [diff]
    type = Diffusion
    variable = u
  []
  [react]
    type = Reaction
    variable = u
  []
[]

[InterfaceKernels]
  # Reads s and ds/du from both sides; dsource_du_neighbor = 0 because side A's s is independent of
  # side B's u (and vice versa). The neighbor-side reads pick up the two-sided-published values.
  [source]
    type = MaterialPropertySource
    variable = u
    neighbor_var = u
    source = 's'
    dsource_du = 'ds/du'
    dsource_du_neighbor = 0
    boundary = 'A_B'
  []
[]

[NEML2]
  input = 'models/custom_model.i'
  device = 'cpu'
  eager = true
  load = 'models/test_models.py'

  input_types = 'VARIABLE'
  inputs = 'u'

  derivatives = 's u'

  [source]
    model = 'interface_source'
    block = 'A B'
    interface = 'A_B'
    interface_only = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  petsc_options_iname = '-pc_type'
  petsc_options_value = 'lu'
  nl_abs_tol = 1e-10
  nl_rel_tol = 1e-08
[]

[Outputs]
  exodus = true
[]
