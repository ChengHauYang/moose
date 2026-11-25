# KokkosBiLinearMixedModeTraction

!syntax description /Materials/KokkosBiLinearMixedModeTraction

This interface material evaluates the mixed-mode bilinear cohesive law of
[BiLinearMixedModeTraction](BiLinearMixedModeTraction.md) on the Kokkos backend. It reads the local
`interface_displacement_jump` and declares the local `interface_traction`, its derivative
`dinterface_traction_djump`, the stateful `damage`, and the stateful `damage_history_jump`, all
prefixed by `base_name`. The strengths and fracture energies accept material property names or
numbers.

`damage_history_jump` is the local jump at which the damage was last evaluated. Its old value
replaces the old `interface_displacement_jump` that the host material uses for
[!param](/Materials/KokkosBiLinearMixedModeTraction/lag_mode_mixity) and
[!param](/Materials/KokkosBiLinearMixedModeTraction/lag_displacement_jump); without
stabilization, the two are equal.

With `stabilization = weighted_traction` in the
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md) action, this law,
with the damage and history jump of the last converged step, is the trial law of the generalized
Robin solve. The law then acts at the auxiliary jump of that solve, so
[KokkosSCZMStabilizationData](KokkosSCZMStabilizationData.md) records the damage and history jump at
the converged auxiliary jump. As the stabilization parameter grows, the auxiliary jump tends to the
interface jump and the solution tends to that of the unstabilized interface.

!syntax parameters /Materials/KokkosBiLinearMixedModeTraction

!syntax inputs /Materials/KokkosBiLinearMixedModeTraction

!syntax children /Materials/KokkosBiLinearMixedModeTraction
