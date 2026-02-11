nx = 64
[Problem]
  solve = false
[]

[Mesh]
  [boundary_mesh]
    type = FileMeshGenerator
    file = 'star.msh'
  []

  [shift_boundary_mesh]
    type = TransformGenerator
    transform = TRANSLATE
    vector_value = '2.0 2.0 0.0' # translation in x, y, z directions
    input = boundary_mesh
    save_with_name = 'shift_boundary_mesh'
  []

  [gen]
    type = CartesianMeshGenerator
    dim = 2
    dx = '4'
    dy = '4'
    ix = '${nx}'
    iy = '${nx}'
    subdomain_id = '1'
  []

  add_subdomain_ids = 2 # outside block
  add_sideset_names = 'SBMinterface'
  final_generator = 'gen'
[]

[UserObjects]
  [TreeBuilder]
    type = SBMSurfaceMeshBuilder
    surface_mesh = shift_boundary_mesh
  []
  [distance_to_surface]
    type = ShortestDistanceToSurface
    surfaces = 'sign_dist_star circle1 circle2 circle3 ellipse_rot ellipse2 box1 box2'
  []
  [signed_distance_to_surface]
    type = ShortestDistanceToSurface
    surfaces = 'sign_dist_star circle1 circle2 circle3 ellipse_rot ellipse2 box1 box2'
    signed_distance = true
  []
  [InOutTest]
    type = PointInPolyhedronCheckUO
    builder = TreeBuilder
  []
[]

[Variables]
  [u]
    initial_condition = 1
    block = 1
  []
[]

[Functions]

  [dist_star]
    type = UnsignedDistanceToSurfaceMesh
    builder = TreeBuilder
  []

  [sign_dist_star]
    type = SignedDistanceToSurfaceMesh
    builder = TreeBuilder
    in_out_test = InOutTest
  []

  [circle1]
    type = ParsedFunction
    expression = "sqrt((x-1.0)^2 + (y-1.0)^2) - 0.5"
  []

  [ellipse_rot]
    type = ParsedFunction
    symbol_names = 'a b theta cx cy'
    symbol_values = '0.8 0.4 0.785398 3.0 1.0'
    expression = "
      (
        (
          ((x-cx)*cos(theta) + (y-cy)*sin(theta)) / a
        )^2
        +
        (
          (-(x-cx)*sin(theta) + (y-cy)*cos(theta)) / b
        )^2
      )^(0.5) - 1
    "
  []

  [circle2]
    type = ParsedFunction
    # Overlap with circle1 (center close to circle1)
    expression = "sqrt((x-1.35)^2 + (y-1.05)^2) - 0.55"
  []

  [circle3]
    type = ParsedFunction
    # Overlap with ellipse_rot and potentially the shifted star region
    expression = "sqrt((x-2.55)^2 + (y-1.30)^2) - 0.65"
  []

  [ellipse2]
    type = ParsedFunction
    # Another rotated ellipse overlapping ellipse_rot
    symbol_names = 'a b theta cx cy'
    symbol_values = '0.9 0.35 -0.523599 2.6 1.1'
    expression = "
      (
        (
          ((x-cx)*cos(theta) + (y-cy)*sin(theta)) / a
        )^2
        +
        (
          (-(x-cx)*sin(theta) + (y-cy)*cos(theta)) / b
        )^2
      )^(0.5) - 1
    "
  []

  [box1]
    type = ParsedFunction
    symbol_names = 'cx cy hx hy eps'
    symbol_values = '2.2 2.0 0.55 0.35 1e-6'
    # smooth box via smoothmax( softabs(x-cx)-hx , softabs(y-cy)-hy )
    expression = "
      0.5 * (
        (sqrt((x-cx)^2 + eps^2) - hx) + (sqrt((y-cy)^2 + eps^2) - hy)
        +
        sqrt( ((sqrt((x-cx)^2 + eps^2) - hx) - (sqrt((y-cy)^2 + eps^2) - hy))^2 + eps^2 )
      )
    "
  []

  [box2]
    type = ParsedFunction
    symbol_names = 'cx cy hx hy eps'
    symbol_values = '1.15 1.15 0.45 0.25 1e-6'
    expression = "
      0.5 * (
        (sqrt((x-cx)^2 + eps^2) - hx) + (sqrt((y-cy)^2 + eps^2) - hy)
        +
        sqrt( ((sqrt((x-cx)^2 + eps^2) - hx) - (sqrt((y-cy)^2 + eps^2) - hy))^2 + eps^2 )
      )
    "
  []
[]

[AuxVariables]
  [distance]
    order = CONSTANT
    family = MONOMIAL
  []
  [signed_distance]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [dist]
    type = ElementCentroidToSurfaceDistanceAux
    distance_to_surface = distance_to_surface
    variable = distance
    execute_on = 'INITIAL timestep_begin'
  []
  [signed_dist]
    type = ElementCentroidToSurfaceDistanceAux
    distance_to_surface = signed_distance_to_surface
    variable = signed_distance
    execute_on = 'INITIAL timestep_begin'
  []
[]

[Executioner]
  type = Steady
[]

[Outputs]
  exodus = true
[]
