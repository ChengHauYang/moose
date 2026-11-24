# ADWaveSpeed

`ADWaveSpeed` computes the approximate wave speed described by [WaveSpeed](WaveSpeed.md) from AD
density and effective-stiffness properties. Here effective stiffness denotes the square root of
the equivalent elastic modulus, so the wave speed is its ratio to the square root of density.

In this example, the two input properties depend on separate nonlinear variables.
[ADMaterialPropertyValue](ADMaterialPropertyValue.md) projects the wave speed to a field variable,
retaining derivatives from both inputs.

!listing modules/solid_mechanics/test/tests/ad_migration/wave_speed.i

!syntax parameters /Materials/ADWaveSpeed

!syntax inputs /Materials/ADWaveSpeed

!syntax children /Materials/ADWaveSpeed
