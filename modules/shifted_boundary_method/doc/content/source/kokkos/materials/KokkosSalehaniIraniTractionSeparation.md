# KokkosSalehaniIraniTractionSeparation

!syntax description /Materials/KokkosSalehaniIraniTractionSeparation

This interface material evaluates the memoryless Salehani-Irani 3DC cohesive law of
[SalehaniIrani3DCTraction](SalehaniIrani3DCTraction.md) on the Kokkos backend. It reads the local
`interface_displacement_jump` and declares the local `interface_traction` and its derivative
`dinterface_traction_djump`, all prefixed by `base_name`. The traction peaks at
`maximum_normal_traction` for a pure opening of `normal_gap_at_maximum_normal_traction` and at
`maximum_shear_traction` for a pure slip of `tangential_gap_at_maximum_shear_traction`; all four
parameters must be positive.

With `stabilization = weighted_traction` in the
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md) action, this law is
also the trial law of the generalized Robin solve. The law is nonlinear, so each quadrature point
iterates its Newton update to convergence.

!syntax parameters /Materials/KokkosSalehaniIraniTractionSeparation

!syntax inputs /Materials/KokkosSalehaniIraniTractionSeparation

!syntax children /Materials/KokkosSalehaniIraniTractionSeparation
