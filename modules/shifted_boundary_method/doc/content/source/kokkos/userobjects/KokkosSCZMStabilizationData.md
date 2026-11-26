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

## Adding a traction-separation law

The generalized Robin solve is independent of the cohesive law: it only queries the local traction
and its derivative with respect to the local jump at trial jumps, and, for a law with history, has
the state recorded at the converged $\mathbf{j}$. A new traction-separation law therefore works with
weighted-traction stabilization, automatic $\beta$, and stress recovery without changes to this
object, the kernels, or the action, provided it meets one of the following contracts.

- +Native Kokkos material:+ derive from `SCZMTrialTractionProvider` and implement `trialLaw(datum, qp)`,
  which returns a per-quadrature-point law with `evaluate(local_jump, ndisp)` and `isAffine()`, and
  `robinPrepass()`, as described in `SCZMTrialTractionProvider.h`. A law with history reads its old
  state in `trialLaw()` and also implements `recordTrialState(datum, qp, local_jump)`, which this
  object detects at compile time and calls at the converged $\mathbf{j}$.
- +NEML2 model:+ no MOOSE code is needed. The model must produce `interface_traction` from the local
  `interface_displacement_jump`, the `[NEML2]` block must request the derivative
  `dinterface_traction_djump` and set `input_backend = kokkos` and `output_backend = kokkos`, and a
  model with history must set [!param](/NEML2/manage_state_advance) to `true`. A model that violates
  this contract is rejected with an explicit error.

For example, the `[NEML2]` block above runs the NEML2 Salehani-Irani law unchanged once its
`input` points to

!listing modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/salehani_traction_neml2.i block=Models

Each NEML2 traction-separation law in the test suite reproduces the native Kokkos law with the same
parameters under weighted-traction stabilization.

## Evaluation

The object is passive with [!param](/UserObjects/KokkosSCZMStabilizationData/execute_on) set to `NONE`. The kernels prepare it for
assembly and invalidate it in `residualSetup()` and `jacobianSetup()`. Residual-only
assembly skips the tangents.

The first evaluation computes and freezes $\beta$ at each quadrature point, scaled by
[!param](/UserObjects/KokkosSCZMStabilizationData/stabilization_gamma), from the normal stiffness
$k = n_i n_J n_k n_L C_{iJkL}$ of the bulk tangent on each side, taken from the face
[!param](/UserObjects/KokkosSCZMStabilizationData/tangent), or from the recovered
[!param](/UserObjects/KokkosSCZMStabilizationData/volume_tangent) with stress recovery. Automatic
$\beta$ must be finite and positive.

If the local Robin solve fails at any quadrature point, the object flags an exception through the
problem so that the nonlinear solve stops and the time step can be cut back, instead of assembling
an invalid traction.

!syntax parameters /UserObjects/KokkosSCZMStabilizationData

!syntax inputs /UserObjects/KokkosSCZMStabilizationData

!syntax children /UserObjects/KokkosSCZMStabilizationData
