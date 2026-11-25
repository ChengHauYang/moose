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

When `stabilization = weighted_traction`, setting `stress_recovery = element_linear` evaluates the
stabilization's bulk PK1 stress at the true-interface quadrature point by recovering each side
independently from its volume quadrature-point properties (`volume_stress`, `volume_tangent`, and
with `volumetric_locking_correction = true`, `volume_F_avg_invT`, `volume_fbar_A`, and
`volume_fbar_gamma_over_3`). On distributed meshes, those volume properties are synchronized from
owning ranks to ghost elements. The same volume properties recover the directional-correction PK1
stress at the surrogate-face quadrature point, removing the need for interface-face bulk stress
evaluations when directional correction is enabled.

!syntax parameters /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

!syntax inputs /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

!syntax children /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian
