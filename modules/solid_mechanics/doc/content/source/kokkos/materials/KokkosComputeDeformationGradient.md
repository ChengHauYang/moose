# KokkosComputeDeformationGradient

!syntax description /Materials/KokkosComputeDeformationGradient

`KokkosComputeDeformationGradient` computes the total deformation gradient
\(\mathbf{F} = \mathbf{I} + \nabla \mathbf{u}\) from Kokkos displacement gradients and stores it
as a Kokkos full rank-two material property. The property can be supplied directly to NEML2 through
the Kokkos input backend.

!syntax parameters /Materials/KokkosComputeDeformationGradient

!syntax inputs /Materials/KokkosComputeDeformationGradient

!syntax children /Materials/KokkosComputeDeformationGradient
