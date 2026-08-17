# Minimal reproducer: EqualValueBoundaryConstraint on a distributed mesh.
#
# EqualValueBoundaryConstraint::ghostPrimary() hand-gathers the elements
# connected to the primary node with allgather_packed_range() and then stores a
# bare element ID in SubProblem::ghostedElems(). Those elements are not held by
# any ghosting functor, so a deleteRemoteElements() call that runs after
# add_constraint drops them again, and SystemBase::augmentSendList() later
# dereferences the now-dangling ID.
#
# The side user object requests a late GhostLowerDElems relationship manager,
# which arms the deletion but retains nothing because this mesh has no
# lower-dimensional elements.
#
# Run with: mpiexec -n 3 ./moose_test-dbg -i minimal_evbc.i

[Mesh]
  parallel_type = DISTRIBUTED
  [gen]
    type = GeneratedMeshGenerator
    dim = 2
    nx = 10
    ny = 10
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
[]

[UserObjects]
  [late_geometric_ghosting]
    type = TestGhostBoundarySideUserObject
    boundary = left
  []
[]

[Constraints]
  [top]
    type = EqualValueBoundaryConstraint
    variable = u
    secondary = top
    primary = 120
    penalty = 1e7
  []
[]

[Problem]
  solve = false
[]

[Executioner]
  type = Steady
[]

[Outputs]
  exodus = false
[]
