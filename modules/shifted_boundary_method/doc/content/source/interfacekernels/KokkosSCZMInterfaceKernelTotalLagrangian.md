# KokkosSCZMInterfaceKernelTotalLagrangian

!syntax description /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

This interface kernel assembles cohesive PK1 traction on a surrogate interface using Kokkos for
total-Lagrangian finite-strain kinematics. It applies the true-to-surrogate reference-area projection
and the directional correction based on bulk PK1 stress. Its Jacobian includes derivatives through both
the shifted displacement jump and the surrogate-interface deformation gradient.

With `volumetric_locking_correction = true`, the directional correction uses the total-mode F-bar
PK1 stress and its exact Jacobian, as described in
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md). The trial-gradient
average over each bulk element is taken from the volume-reinitialized assembly data of that element.

## Weighted-traction stabilization

With `stabilization = weighted_traction`, the kernel solves the generalized Robin equation

!equation
\mathbf{j} + \frac{\mathbf{f}(\mathbf{j})}{\beta}
= \mathbf{J}_d + \frac{\mathbf{T}_h}{\beta}, \qquad
\widehat{\mathbf{t}} = \mathbf{f}(\mathbf{j}),

at each paired interface quadrature point. Here $\mathbf{J}_d$ is the shifted displacement jump,
$\mathbf{T}_h$ is the average adjacent bulk PK1 traction, and $\mathbf{f}$ is the cohesive law
evaluated at the auxiliary jump $\mathbf{j}$. If
$\mathbf{K} = \partial\mathbf{f}/\partial\mathbf{j}$ and
$\mathbf{A} = \mathbf{I} + \mathbf{K}/\beta$, the consistent derivatives include
$\partial\widehat{\mathbf{t}}/\partial\mathbf{J}_d = \mathbf{K}\mathbf{A}^{-1}$ and
$\partial\widehat{\mathbf{t}}/\partial\mathbf{T}_h =
\mathbf{K}\mathbf{A}^{-1}/\beta$. The assembled Jacobian also includes the exact shifted-geometry,
Nanson, bulk-stress, and F-bar derivatives in all four interface blocks.

An explicitly supplied `stabilization_beta` is used directly. Otherwise the
[KokkosSCZMStabilizationData](KokkosSCZMStabilizationData.md) user object derives beta on its first
preparation and freezes it:

!equation
\beta = \frac{\gamma}{2}\left(\frac{\kappa_N^-}{h_N^-}
+ \frac{\kappa_N^+}{h_N^+}\right), \qquad h_N^\pm = \frac{V^\pm}{A},

where `stabilization_gamma` is $\gamma$ and defaults to 5. The normal directional stiffness
$\kappa_N^\pm$ is computed from each adjacent element's `volume_tangent`; this property is requested
on ghost elements for distributed meshes. Automatic beta is rejected if the derived value is not
finite and positive. The deprecated `stabilization_normal_stiffness` and
`stabilization_tangential_stiffness` parameters are not used by this total-Lagrangian generalized
formulation.

The native device evaluator selected by `stabilization_cohesive_law` supports `affine` and
`coupled_cubic`. The latter evaluates
$\mathbf{f}(\mathbf{j}) = k_0\mathbf{j} + a\lVert\mathbf{j}\rVert^2\mathbf{j} + \mathbf{c}$
and requires nonnegative `stabilization_linear_stiffness` and
`stabilization_cubic_stiffness`. This trial-evaluation path is currently limited to native,
memoryless monotone laws. Stateful NEML2 cohesive laws require a batched trial-evaluation adapter
and are not silently approximated from their current tangent. The local damped Newton solve reports
failure rather than accepting an invalid constitutive evaluation, singular or ill-conditioned Robin
tangent, unsuccessful line search, or unconverged branch.

Setting `stress_recovery = element_linear` evaluates the stabilization's bulk PK1 stress at the
true-interface quadrature point by recovering each side independently from its volume
quadrature-point properties (`volume_stress`, `volume_tangent`, and with
`volumetric_locking_correction = true`, `volume_F_avg_invT`, `volume_fbar_A`, and
`volume_fbar_gamma_over_3`). On distributed meshes, those volume properties are synchronized from
owning ranks to ghost elements. The same volume properties recover the directional-correction PK1
stress at the surrogate-face quadrature point, removing the need for interface-face bulk stress
evaluations when directional correction is enabled.

!syntax parameters /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

!syntax inputs /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

!syntax children /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian
