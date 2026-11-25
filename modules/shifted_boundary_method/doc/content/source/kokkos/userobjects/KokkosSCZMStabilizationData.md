# KokkosSCZMStabilizationData

!syntax description /UserObjects/KokkosSCZMStabilizationData

`KokkosSCZMStabilizationData` evaluates weighted-traction stabilization once per assembly
for [small-strain](KokkosSCZMInterfaceKernelSmallStrain.md) and
[total-Lagrangian](KokkosSCZMInterfaceKernelTotalLagrangian.md) kernels. The
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md) action creates it
when [!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stabilization) is set to `weighted_traction` and passes it to each displacement
component kernel through its [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain/stabilization_data) parameter.

At each paired interface quadrature point, the object computes the average adjacent bulk traction
$\mathbf{T}_h$ from Cauchy stress for small strain or PK1 stress for total Lagrangian and solves
the [generalized Robin equation](KokkosSCZMInterfaceKernelTotalLagrangian.md#weighted-traction-stabilization). It
stores the stabilized traction $\widehat{\mathbf{t}}$ and the auxiliary jump $\mathbf{j}$. When the
assembly needs a Jacobian, it also stores
$\mathbf{W} = \partial\widehat{\mathbf{t}}/\partial\mathbf{J}_d$, the inverse Robin tangent
$\mathbf{S}$, and, for total Lagrangian, the cohesive traction derivative with respect to the
surrogate-interface deformation gradient left-multiplied by $\mathbf{S}$. Each kernel reads these
entries using the interface face's quadrature-point offset.

The cohesive law acts at $\mathbf{j}$ rather than at the interface jump, so a stateful
traction-separation material such as
[KokkosBiLinearMixedModeTraction](KokkosBiLinearMixedModeTraction.md) records its history at the
converged $\mathbf{j}$ of each quadrature point instead of in its own property evaluation.

An interface `[NEML2]` traction-separation model that supplies the cohesive traction is itself
the trial law: each Robin iteration evaluates it for the whole batch at the trial jumps. A model
with history, i.e. one that reads an old value such as `interface_displacement_jump~1` or
auto-declared damage, must set
[!param](/NEML2/manage_state_advance) to `true`. Once every quadrature point
converges, the model is re-evaluated at $\mathbf{j}$, and the executor advances its state from
that evaluation at the end of the step. For example, the NEML2 version of the mixed-mode bilinear
law

!listing modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/bilinear_traction_neml2.i block=Models/traction

is coupled with

!listing modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/sczm_bilinear_neml2.i block=NEML2

The object is passive with [!param](/UserObjects/KokkosSCZMStabilizationData/execute_on) set to `NONE`. The kernels prepare it for
assembly and invalidate it in `residualSetup()` and `jacobianSetup()`. Residual-only
assembly skips the tangents.

Supply [!param](/UserObjects/KokkosSCZMStabilizationData/stabilization_beta) for a fixed $\beta$. Otherwise the first evaluation computes
and freezes $\beta$ at each quadrature point using the adjacent elements'
[!param](/UserObjects/KokkosSCZMStabilizationData/volume_tangent). Automatic $\beta$ must be finite and positive.

If the local Robin solve fails at any quadrature point, the object flags an exception through the
problem so that the nonlinear solve stops and the time step can be cut back, instead of assembling
an invalid traction.

!syntax parameters /UserObjects/KokkosSCZMStabilizationData

!syntax inputs /UserObjects/KokkosSCZMStabilizationData

!syntax children /UserObjects/KokkosSCZMStabilizationData
