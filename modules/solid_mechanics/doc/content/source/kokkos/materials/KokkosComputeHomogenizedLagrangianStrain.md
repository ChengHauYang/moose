# KokkosComputeHomogenizedLagrangianStrain

!if! function=hasCapability('kokkos')

`KokkosComputeHomogenizedLagrangianStrain` is the Kokkos version of
[ComputeHomogenizedLagrangianStrain](ComputeHomogenizedLagrangianStrain.md). It writes the
components of the macro gradient scalar variable into a rank-two Kokkos material property through
the homogenization constraint map. Pass the property to the `homogenization_gradient_names` of
[KokkosComputeDeformationGradient](KokkosComputeDeformationGradient.md) and
[KokkosComputeLagrangianStressCustomPK2](KokkosComputeLagrangianStressCustomPK2.md).

## Example Input Syntax

!listing solid_mechanics/test/tests/kokkos/homogenization/small_2d.i block=Materials/compute_homogenization_gradient

!syntax parameters /Materials/KokkosComputeHomogenizedLagrangianStrain

!syntax inputs /Materials/KokkosComputeHomogenizedLagrangianStrain

!syntax children /Materials/KokkosComputeHomogenizedLagrangianStrain

!if-end!

!else
!include kokkos/kokkos_warning.md
