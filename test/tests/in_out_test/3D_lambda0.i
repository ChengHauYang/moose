nx = 32
[Problem]
  solve = false
[]

[Mesh]
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
[]

[UserObjects]
  [boundary_mesh]
    type = WaterTightGeoReader
    mesh_file = bunny.msh
    x_shift = 2.0
    y_shift = 2.0
    z_shift = 2.0
  []
[]

[MeshModifiers]
  [IntercpetedESM]
    type = InterceptedElementModifier
    subdomain_id_inside = 1
    subdomain_id_outside = 2
    lambda = 0
    outer_boundary = true
    water_tight_geo_reader = boundary_mesh
    execute_on = 'INITIAL'
    execution_order_group = 0
  []
[]

[Variables]
  [u]
    initial_condition = 1
  []
[]

[Executioner]
  type = Steady
[]

[Outputs]
  exodus = true
[]
