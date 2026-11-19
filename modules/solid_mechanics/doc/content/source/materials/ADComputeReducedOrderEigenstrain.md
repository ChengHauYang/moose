# ComputeReducedOrderEigenstrain / ADComputeReducedOrderEigenstrain

!syntax description /Materials/ADComputeReducedOrderEigenstrain

## Description

`ADComputeReducedOrderEigenstrain` reduces one or more AD eigenstrain fields to the polynomial order
of the compatible strain field while preserving derivatives of the input eigenstrains. On first-order
elements it computes an element-wise volume average. On second-order elements it computes a linear
least-squares projection.

Use [!param](/Materials/ADComputeReducedOrderEigenstrain/input_eigenstrain_names) to list the input
eigenstrain properties. The result is available under the name selected by
[!param](/Materials/ADComputeReducedOrderEigenstrain/eigenstrain_name).

## Example Input Syntax

In this example, a nonlinear concentration-dependent eigenstrain is reduced before it enters an AD
linear-elastic stress calculation. The test runs this input with first- and second-order elements to
exercise both reduction methods.

!listing modules/solid_mechanics/test/tests/ad_migration/reduced_order_eigenstrain.i block=Materials

!syntax parameters /Materials/ADComputeReducedOrderEigenstrain

!syntax inputs /Materials/ADComputeReducedOrderEigenstrain

!syntax children /Materials/ADComputeReducedOrderEigenstrain
