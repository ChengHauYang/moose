# KokkosComputeSmallStrain

!syntax description /Materials/KokkosComputeSmallStrain

`KokkosComputeSmallStrain` computes the infinitesimal strain tensor from the displacement gradients
and stores it as a Kokkos material property. The property can be supplied to a NEML2 model through
the Kokkos input backend, which packs its symmetric components in Mandel order
`(xx, yy, zz, yz, xz, xy)`.

!syntax parameters /Materials/KokkosComputeSmallStrain

!syntax inputs /Materials/KokkosComputeSmallStrain

!syntax children /Materials/KokkosComputeSmallStrain
