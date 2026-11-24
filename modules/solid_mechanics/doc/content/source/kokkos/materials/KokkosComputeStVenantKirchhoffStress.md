# KokkosComputeStVenantKirchhoffStress

!if! function=hasCapability('kokkos')

`KokkosComputeStVenantKirchhoffStress` computes the isotropic St. Venant-Kirchhoff PK2 stress

\[
S = \lambda \operatorname{tr}(E) I + 2 \mu E,
\qquad
E = \frac{1}{2}\left(F^T F - I\right),
\]

and its derivative with respect to the deformation gradient $F$ as Kokkos material properties,
the inputs of [KokkosComputeLagrangianStressCustomPK2](KokkosComputeLagrangianStressCustomPK2.md).
It matches the large kinematics branch of the host
[ComputeStVenantKirchhoffStress](ComputeStVenantKirchhoffStress.md) with an isotropic elasticity
tensor.

## Example Input Syntax

!listing solid_mechanics/test/tests/kokkos/homogenization/large_2d.i block=Materials/elastic_1

!syntax parameters /Materials/KokkosComputeStVenantKirchhoffStress

!syntax inputs /Materials/KokkosComputeStVenantKirchhoffStress

!syntax children /Materials/KokkosComputeStVenantKirchhoffStress

!if-end!

!else
!include kokkos/kokkos_warning.md
