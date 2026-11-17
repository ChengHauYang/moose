# ADOutOfPlanePressure

`ADOutOfPlanePressure` applies the prescribed load described by
[OutOfPlanePressure](OutOfPlanePressure.md) in an AD residual. The prescribed
[!param](/Kernels/ADOutOfPlanePressure/function),
[!param](/Kernels/ADOutOfPlanePressure/factor), and optional
[!param](/Kernels/ADOutOfPlanePressure/postprocessor) are multiplied together.
Function and postprocessor values remain non-AD inputs.

In this example on the undisplaced mesh, a reaction term balances the applied pressure.
The resulting field has a known linear solution, allowing the AD and non-AD kernels to be
compared against the same reference values.

!listing modules/solid_mechanics/test/tests/ad_migration/out_of_plane_pressure.i

!syntax parameters /Kernels/ADOutOfPlanePressure

!syntax inputs /Kernels/ADOutOfPlanePressure

!syntax children /Kernels/ADOutOfPlanePressure
