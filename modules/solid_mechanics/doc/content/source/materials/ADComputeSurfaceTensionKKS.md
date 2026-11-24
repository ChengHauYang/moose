# ADComputeSurfaceTensionKKS

`ADComputeSurfaceTensionKKS` computes the surface-tension tensor described by
[ComputeSurfaceTensionKKS](ComputeSurfaceTensionKKS.md). Its barrier and gradient-energy properties
must be AD properties, and their derivatives and the order-parameter gradient derivatives propagate
to the stress output.

In this example, both energy properties depend on nonlinear variables. The surface stress is
assembled by a separate [ADStressDivergenceTensors](ADStressDivergenceTensors.md) contribution, with
[!param](/Materials/ADComputeSurfaceTensionKKS/planar_stress_name) naming the stress property selected
by [!param](/Kernels/ADStressDivergenceTensors/base_name).

!listing modules/solid_mechanics/test/tests/ad_migration/surface_tension.i

!syntax parameters /Materials/ADComputeSurfaceTensionKKS

!syntax inputs /Materials/ADComputeSurfaceTensionKKS

!syntax children /Materials/ADComputeSurfaceTensionKKS
