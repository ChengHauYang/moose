# KokkosSCZMStabilizationData

!syntax description /UserObjects/KokkosSCZMStabilizationData

This interface user object evaluates the weighted-traction stabilization of a total-Lagrangian
shifted cohesive zone once per assembly and stores the result for
[KokkosSCZMInterfaceKernelTotalLagrangian](KokkosSCZMInterfaceKernelTotalLagrangian.md). The
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md) action creates it
when `stabilization = weighted_traction` and passes it to the kernel through `stabilization_data`;
it is not normally added to an input file directly.

At each paired interface quadrature point, the object computes the average adjacent bulk PK1
traction $\mathbf{T}_h$ and solves the generalized Robin equation described on the kernel page. It
stores the stabilized traction $\widehat{\mathbf{t}}$ and the auxiliary jump $\mathbf{j}$. When the
assembly needs a Jacobian, it also stores
$\mathbf{W} = \partial\widehat{\mathbf{t}}/\partial\mathbf{J}_d$, the inverse Robin tangent
$\mathbf{A}^{-1}$, and $\mathbf{A}^{-1}$ applied to the derivative of the cohesive traction with
respect to the surrogate-interface deformation gradient. Entries are indexed by the interface face's
quadrature-point offset, so the kernel reads them without recomputing the local solve for each test
and trial function.

The object uses `execute_on = NONE`. The kernel prepares it at the start of each residual or
Jacobian computation and invalidates it in `residualSetup()` and `jacobianSetup()`. Residual-only
passes skip the tangents.

If `stabilization_beta` is omitted, the first preparation computes beta at every quadrature point
from the adjacent `volume_tangent` and freezes it. A beta that is not finite and positive is an
error.

If the local Robin solve fails at any quadrature point, the object flags an exception through the
problem so that the nonlinear solve stops and the time step can be cut back, instead of assembling
an invalid traction.

!syntax parameters /UserObjects/KokkosSCZMStabilizationData

!syntax inputs /UserObjects/KokkosSCZMStabilizationData

!syntax children /UserObjects/KokkosSCZMStabilizationData
