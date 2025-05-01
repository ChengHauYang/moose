[Mesh]
  [gmg]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 200
    ny = 200
  []
  [solid]
    type = SubdomainBoundingBoxGenerator
    input = gmg
    block_id = 0
    block_name = solid
    bottom_left = '0 0 0'
    top_right = '1 1 0'
  []
  [liquid]
    type = SubdomainBoundingBoxGenerator
    input = solid
    block_id = 1
    block_name = liquid
    bottom_left = '0.7 0.5 0'
    top_right = '0.71 0.51 0'
  []
  use_displaced_mesh = false
[]

[Problem]
  solve = false
[]

[AuxVariables]
  [u]
  []
[]

[SpatioTemporalPaths]
  [path]
    type = CSVPiecewiseLinearSpatioTemporalPath
    file = 'concentric_circles.csv'
    #update_interval = 1
    #file = 'outward_spiral.csv'
    verbose = true
  []
[]

[UserObjects]
  [esm]
    type = SpatioTemporalPathElementSubdomainModifier
    path = 'path'
    radius = 0.03
    target_subdomain = 'liquid'
    #radius_based = true
    execute_on = 'INITIAL TIMESTEP_BEGIN'
  []
[]

[Executioner]
  type = Transient
  dt = 1
  end_time = 1000
[]

[Outputs]
  exodus = true
[]
