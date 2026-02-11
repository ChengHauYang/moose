nx = 32
[Problem]
  solve = false
[]

[Mesh]
  [boundary_mesh]
    type = FileMeshGenerator
    file = 'bunny.msh'
  []

  [shift_boundary_mesh]
    type = TransformGenerator
    transform = TRANSLATE
    vector_value = '2.0 2.0 2.0' # translation in x, y, z directions
    input = boundary_mesh
    save_with_name = 'shift_boundary_mesh'
  []

  [gen]
    type = CartesianMeshGenerator
    dim = 3
    dx = '4'
    dy = '4'
    dz = '4'
    ix = '${nx}'
    iy = '${nx}'
    iz = '${nx}'
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
  [inout_test]
    type = PointInPolyhedronCheckUO
    # ray_direction = '0 1 0'
    builder = TreeBuilder
    obb_file_name = '3D_sbm_lambda1/obb.vtk'
    ray_file_name = '3D_sbm_lambda1/ray.vtk'
  []
[]

[MeshModifiers]
  [IntercpetedESM]
    type = InterceptedElementModifier
    subdomain_id_inside = 1
    subdomain_id_outside = 2
    lambda = 1
    outer_boundary = true
    in_out_test = inout_test
    execute_on = 'INITIAL'
    execution_order_group = 0
  []
  [SBMinterface]
    type = SidesetAroundSubdomainUpdater
    inner_subdomains = 1
    outer_subdomains = 2
    update_sideset_name = 'SBMinterface'
    assign_outer_surface_sides = false
    execute_on = 'INITIAL'
    execution_order_group = 1
    block = '1 2' # need to override default block
  []
[]

[Variables]
  [u]
    initial_condition = 1
    block = 1
  []
[]

[Executioner]
  type = Steady
[]

[Outputs]
  [pgraph]
    type = PerfGraphOutput
    execute_on = 'initial final' # Default is "final"
    level = 2 # Default is 1
    heaviest_branch = true # Default is false
    heaviest_sections = 7 # Default is 0
  []
  exodus = true
  perf_graph = true
[]


