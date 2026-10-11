# KokkosSCZMInterfaceKernelSmallStrain

!syntax description /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

This kernel assembles the small-strain SCZM traction-jump contribution on the Kokkos
backend. It consumes device-resident global traction and its derivative with respect
to the global shifted displacement jump. The properties must be produced by compatible
Kokkos materials; this kernel does not evaluate a cohesive traction law itself.

The kernel derives from the generic Kokkos shifted-interface base, which caches static
surrogate-to-true geometry from `sbm_distance_uo`. It assembles both residual rows and
all four interface Jacobian blocks, including cross-component displacement coupling.

## Material Properties

The `traction_global` property is a `MaterialProperty<Real, 1>` containing the global
traction vector. The `dtraction_djump_global` property is a `MaterialProperty<Real, 2>`
containing the global traction tangent. `base_name` prefixes these property names.
The number of displacement variables must match the mesh dimension.

## Limitations

Directional correction, consistent bulk-stress tangents, volumetric locking correction,
and `no_shifted` are not implemented. The production Kokkos displacement-jump, rotation,
and cohesive traction material chain is also incomplete.

The [ShiftedCohesiveZone action](ShiftedCohesiveZoneAction.md) currently selects host
objects. Focused tests instantiate the Kokkos kernel directly with a test-only linear
traction law; these tests establish assembly and linearization, not a production
constitutive pipeline or end-to-end GPU residency.

!syntax parameters /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

!syntax inputs /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain

!syntax children /InterfaceKernels/KokkosSCZMInterfaceKernelSmallStrain
