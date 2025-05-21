nx = 64
[Problem]
  solve = false
[]

[Mesh]
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
[]

[UserObjects]
  [boundary_mesh]
    type = WaterTightGeoReader
    mesh_file = star.msh
    x_shift = 2.0
    y_shift = 2.0
  []
[]

[MeshModifiers]
  [IntercpetedESM]
    type = InterceptedElementModifier
    subdomain_id_inside = 1
    subdomain_id_outside = 2
    lambda = 1
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
