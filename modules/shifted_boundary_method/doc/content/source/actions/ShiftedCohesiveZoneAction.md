# ShiftedCohesiveZoneAction

!syntax description /Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction

The `ShiftedCohesiveZoneAction` creates the interface kernels, materials, and geometry objects for
the [Shifted Cohesive Zone Physics](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md).

The [!param](/Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction/stress_recovery)
parameter optionally evaluates the weighted-stabilization bulk first Piola-Kirchhoff stress at the
true-interface quadrature point. The default `none` uses the interface-quadrature-point stress.
`element_linear` instead recovers each interface side independently from that side's volume
quadrature points. It uses the analytical affine HEX8 weights when the element has eight volume
quadrature points and a general affine least-squares recovery otherwise. Rank-deficient and
non-finite recovery configurations produce an error. The same independent recovery evaluates the
directional-correction stress at the surrogate-face quadrature point. On distributed meshes,
required volume material properties are copied from owning ranks to ghost elements without adding
material evaluations or history points.
This option requires Kokkos total-Lagrangian kinematics, a shifted true-interface mapping, and
weighted traction stabilization.

!syntax parameters /Physics/SolidMechanics/ShiftedCohesiveZone/ShiftedCohesiveZoneAction
