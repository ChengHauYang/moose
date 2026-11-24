# KokkosRankTwoCartesianComponent

!if! function=hasCapability('kokkos')

`KokkosRankTwoCartesianComponent` is the Kokkos version of
[RankTwoCartesianComponent](RankTwoCartesianComponent.md): it copies the $(i, j)$ component of a
rank-two Kokkos material property into a scalar Kokkos material property, for example to average a
stress component with
[KokkosElementAverageMaterialProperty](KokkosElementAverageMaterialProperty.md).

## Example Input Syntax

!listing solid_mechanics/test/tests/kokkos/homogenization/small_2d.i block=Materials/prop_sxx

!syntax parameters /Materials/KokkosRankTwoCartesianComponent

!syntax inputs /Materials/KokkosRankTwoCartesianComponent

!syntax children /Materials/KokkosRankTwoCartesianComponent

!if-end!

!else
!include kokkos/kokkos_warning.md
