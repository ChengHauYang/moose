# SCZMInterfaceKernelTotalLagrangian

!syntax description /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

This interface kernel assembles cohesive PK1 traction on a surrogate interface. It applies the
true-to-surrogate reference-area projection and the directional correction based on bulk PK1
stress. Its non-AD Jacobian includes derivatives through both the shifted displacement jump and the
surrogate-interface deformation gradient.

With `volumetric_locking_correction = true`, the directional correction uses the total-mode F-bar
PK1 stress and its exact Jacobian, as described in
[ShiftedCohesiveZone](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md). The trial-gradient
average over each bulk element is integrated with a separate volume finite element reinitialized on
that element.

!syntax parameters /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

!syntax inputs /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

!syntax children /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian
