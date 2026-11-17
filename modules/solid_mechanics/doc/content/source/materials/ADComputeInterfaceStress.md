# ADComputeInterfaceStress

`ADComputeInterfaceStress` computes the interfacial stress described by
[ComputeInterfaceStress](ComputeInterfaceStress.md), preserving derivatives of every coupled
order-parameter gradient.

Set [!param](/Materials/ADComputeInterfaceStress/planar_stress_name) to a stress property consumed by
[ADStressDivergenceTensors](ADStressDivergenceTensors.md). In this example, the interface stress is
assembled as a separate contribution alongside the elastic stress, using
[!param](/Kernels/ADStressDivergenceTensors/base_name) to select each stress property.

!listing modules/solid_mechanics/test/tests/ad_migration/interface_stress.i

!syntax parameters /Materials/ADComputeInterfaceStress

!syntax inputs /Materials/ADComputeInterfaceStress

!syntax children /Materials/ADComputeInterfaceStress
