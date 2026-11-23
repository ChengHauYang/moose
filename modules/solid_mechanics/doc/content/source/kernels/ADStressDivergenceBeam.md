# ADStressDivergenceBeam

!syntax description /Kernels/ADStressDivergenceBeam

## Description

`ADStressDivergenceBeam` is the automatic-differentiation counterpart of
[StressDivergenceBeam](/StressDivergenceBeam.md). It computes the $i^{th}$ component of
internal forces or moments for a beam element using generic AD material properties so that
derivatives with respect to all coupled variables — including temperature and prescribed
eigenstrain variables — are propagated automatically to the residual and Jacobian. The Jacobian
is assembled through `computeResidualAndJacobian()` in a single pass without any hand-coded
analytic stiffness terms.

For the mathematical formulation and beam kinematics refer to
[C0 Timoshenko Beam](/C0TimoshenkoBeam.md).

!syntax parameters /Kernels/ADStressDivergenceBeam

!syntax inputs /Kernels/ADStressDivergenceBeam

!syntax children /Kernels/ADStressDivergenceBeam
