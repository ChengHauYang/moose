# ADDensityScaling

`ADDensityScaling` computes the scaled and additional inertial densities described by
[DensityScaling](DensityScaling.md), preserving derivatives of the true density and effective
stiffness. Both input properties must be AD properties. The scaled density uses the stable-density
formula when it exceeds the true density; otherwise it retains the true density and its derivatives.

In this example, the scaled density supplies [ADInertialForce](InertialForce.md), while
[ADMaterialPropertyValue](ADMaterialPropertyValue.md) projects the additional density. Nonlinear
density and stiffness inputs exercise both outputs. The scaling formula uses the undisplaced
element size.

!listing modules/solid_mechanics/test/tests/ad_migration/density_scaling.i

!syntax parameters /Materials/ADDensityScaling

!syntax inputs /Materials/ADDensityScaling

!syntax children /Materials/ADDensityScaling
