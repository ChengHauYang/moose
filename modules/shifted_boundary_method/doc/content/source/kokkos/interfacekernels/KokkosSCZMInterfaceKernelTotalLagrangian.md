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

## Weighted-traction stabilization id=weighted-traction-stabilization

With [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/stabilization) set to `weighted_traction`, the shared
[KokkosSCZMStabilizationData](KokkosSCZMStabilizationData.md) user object solves the generalized
Robin equation once per assembly at each paired interface quadrature point.

!equation
\mathbf{j} + \frac{\mathbf{f}(\mathbf{j})}{\beta}
= \mathbf{J}_d + \frac{\mathbf{T}_h}{\beta}, \qquad
\widehat{\mathbf{t}} = \mathbf{f}(\mathbf{j}),

Here $\mathbf{J}_d$ is the shifted displacement jump, $\mathbf{f}$ is the cohesive law evaluated
at the auxiliary jump $\mathbf{j}$, and
$\mathbf{T}_h = \frac{1}{2}(\mathbf{P}^-+\mathbf{P}^+)\mathbf{N}$ is the average adjacent bulk
PK1 traction on the [reference normal](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md#weighted-traction-normal).
For $\mathbf{K} = \partial\mathbf{f}/\partial\mathbf{j}$, the derivatives are

!equation
\mathbf{S} = (\mathbf{I} + \mathbf{K}/\beta)^{-1}, \qquad
\mathbf{W} = \mathbf{S}\mathbf{K}, \qquad
\frac{\partial\widehat{\mathbf{t}}}{\partial\mathbf{J}_d} = \mathbf{W}, \qquad
\frac{\partial\widehat{\mathbf{t}}}{\partial\mathbf{T}_h} = \frac{\mathbf{W}}{\beta}.

The assembled Jacobian also includes the shifted-geometry, Nanson, bulk-stress, and F-bar
derivatives in all four interface blocks.

The user object derives $\beta$ on its first evaluation and freezes it for subsequent assemblies.

!equation
\beta = \frac{\gamma A}{2}\left(\frac{\kappa_N^-}{V^-}
+ \frac{\kappa_N^+}{V^+}\right), \qquad
\kappa_N^\pm = N_i N_J C^\pm_{iJkL} N_k N_L.

Here $A$ is the surrogate face area and $V^\pm$ are the adjacent element volumes.
[!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/stabilization_gamma) sets $\gamma$
(default 5). Each $\mathbf{C}^\pm$ is the face `tangent` of its side, or the
[!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/volume_tangent) recovered at the
true-interface quadrature point when using element-linear stress recovery. Automatic $\beta$ must
be finite and positive.

The Robin solve evaluates the traction-separation law itself at each trial jump, native or NEML2,
so its Jacobian is exact for any law that supplies its jump derivative; see
[KokkosSCZMStabilizationData](KokkosSCZMStabilizationData.md). The local damped Newton solve
reports failure on an invalid constitutive evaluation, singular or ill-conditioned Robin tangent,
unsuccessful line search, or lack of convergence.

Setting [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/stress_recovery) to
`element_linear` evaluates the stabilization's bulk PK1 stress at the true-interface
quadrature point by recovering each side independently from its volume
quadrature-point properties ([!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/volume_stress), [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/volume_tangent), and with
[!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/volumetric_locking_correction) enabled, [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/volume_F_avg_invT),
[!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/volume_fbar_A), and [!param](/InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian/volume_fbar_gamma_over_3)). On distributed meshes,
those volume properties are synchronized from owning ranks to ghost elements. The same volume
properties recover the directional-correction PK1 stress at the surrogate-face quadrature point
and replace interface-face bulk stress evaluations.

!syntax parameters /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

!syntax inputs /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

!syntax children /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian
