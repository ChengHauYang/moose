# Diffusion of u (v) on a square with zero value on the left (bottom) boundary and a unit flux
# through the right (top) boundary. A stiffer block in one quadrant would make u (v) vary along the
# loaded boundary; the constraints make u uniform on the right and v uniform on the top boundary.

diffusion = Diffusion
dirichlet = DirichletBC
neumann = NeumannBC

[Mesh]
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 8
    ny = 8
  []
  [stiff]
    type = SubdomainBoundingBoxGenerator
    input = gen
    bottom_left = '0.5 0 0'
    top_right = '1 0.5 0'
    block_id = 1
  []
[]

[Variables]
  [u]
  []
  [v]
  []
[]

[Kernels]
  [diff_u]
    type = ${diffusion}
    variable = u
  []
  [diff_v]
    type = ${diffusion}
    variable = v
  []
  [diff_u_stiff]
    type = ${diffusion}
    variable = u
    block = 1
  []
  [diff_v_stiff]
    type = ${diffusion}
    variable = v
    block = 1
  []
[]

[BCs]
  [u_left]
    type = ${dirichlet}
    variable = u
    boundary = left
    value = 0
  []
  [u_right]
    type = ${neumann}
    variable = u
    boundary = right
    value = 1
  []
  [v_bottom]
    type = ${dirichlet}
    variable = v
    boundary = bottom
    value = 0
  []
  [v_top]
    type = ${neumann}
    variable = v
    boundary = top
    value = 1
  []
[]

[UserObjects]
  [equal_value]
    type = EqualValueBoundaryDofConstraint
    boundary = 'right top'
    variable = 'u v'
  []
[]

[Postprocessors]
  [u_right_min]
    type = NodalExtremeValue
    variable = u
    boundary = right
    value_type = min
  []
  [u_right_max]
    type = NodalExtremeValue
    variable = u
    boundary = right
    value_type = max
  []
  [v_top_min]
    type = NodalExtremeValue
    variable = v
    boundary = top
    value_type = min
  []
  [v_top_max]
    type = NodalExtremeValue
    variable = v
    boundary = top
    value_type = max
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  nl_abs_tol = 1e-12
[]

[Outputs]
  csv = true
[]
