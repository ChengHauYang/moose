# EqualValueBoundaryDofConstraint

!syntax description /UserObjects/EqualValueBoundaryDofConstraint

## Description

`EqualValueBoundaryDofConstraint` makes a scalar Lagrange variable uniform on each boundary in
`boundary`. It adds one libMesh DOF constraint per boundary node,

!equation
u_i = u_p,

that ties the degree of freedom $u_i$ at the node to the one at the primary node $p$, the boundary
node with the lowest ID. The constraints are applied by the `DofMap` in the same way as periodic
boundary conditions, so they are exact and both the host and the Kokkos assembly condense every
residual and Jacobian contribution of a constrained DOF onto the primary DOF. The primary DOF keeps
an ordinary equation: a load integrated over the boundary, for example a `NeumannBC`, becomes the
total load on the uniform boundary value. In solid mechanics, constraining the normal displacement
of a face keeps the face planar while its position is set by the total force on it, which is the
"plane sections remain plane" boundary condition commonly used for representative volume elements.

Unlike the penalty [EqualValueBoundaryConstraint.md], this object adds no residual object and
no penalty parameter, and it works with Kokkos assembly.

libMesh accepts one constraint object per system, so one `EqualValueBoundaryDofConstraint` must
list every boundary of a system, with the variable on each boundary given in `variable` by
position. Do not apply a Dirichlet boundary condition to constrained DOFs other than the primary
one.

## Example Input Syntax

!listing test/tests/userobjects/equal_value_boundary_dof_constraint/equal_value.i block=UserObjects

!syntax parameters /UserObjects/EqualValueBoundaryDofConstraint

!syntax inputs /UserObjects/EqualValueBoundaryDofConstraint

!syntax children /UserObjects/EqualValueBoundaryDofConstraint
