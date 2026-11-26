# SCZMInterfaceKernelSmallStrain

!syntax description /InterfaceKernels/SCZMInterfaceKernelSmallStrain

The `SCZMInterfaceKernelSmallStrain` assembles cohesive traction on a surrogate interface for
small-strain problems. It evaluates the displacement jump at the true interface and can include a
directional correction based on the bulk stress and constitutive tangent. This object is normally
created by the [ShiftedCohesiveZone action](ShiftedCohesiveZoneAction.md).

## Weighted-traction stabilization

Set [!param](/InterfaceKernels/SCZMInterfaceKernelSmallStrain/stabilization) to `weighted_traction`
to assemble the cohesive traction at the auxiliary jump of the
[generalized Robin equation](syntax/Physics/SolidMechanics/ShiftedCohesiveZone/index.md#weighted-traction-stabilization),
with the average adjacent bulk Cauchy traction. The paired displacement-jump material solves that
equation and supplies the inverse Robin tangent with which this kernel weights the cohesive traction
derivatives. The material, not the kernel, takes `stabilization_gamma`.

!syntax parameters /InterfaceKernels/SCZMInterfaceKernelSmallStrain

!syntax inputs /InterfaceKernels/SCZMInterfaceKernelSmallStrain

!syntax children /InterfaceKernels/SCZMInterfaceKernelSmallStrain
