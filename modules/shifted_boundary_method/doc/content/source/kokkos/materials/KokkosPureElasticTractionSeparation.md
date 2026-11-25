# KokkosPureElasticTractionSeparation

!syntax description /Materials/KokkosPureElasticTractionSeparation

This interface material evaluates a memoryless cohesive law on the Kokkos backend. It reads the local
`interface_displacement_jump` $\mathbf{j}$ and declares the local `interface_traction` and its
derivative `dinterface_traction_djump`, all prefixed by `base_name`. With `cohesive_law = affine`
(the default), the traction is $\mathbf{t} = \mathbf{K}\mathbf{j}$ with the diagonal stiffness
$\mathbf{K}$ set by `normal_stiffness` in the first (normal) local component and
`tangent_stiffness` in the tangential components. With `cohesive_law = coupled_cubic`, the traction
is $\mathbf{t} = (k_0 + a\lVert\mathbf{j}\rVert^2)\mathbf{j}$, where `linear_stiffness` is $k_0$ and
`cubic_stiffness` is $a$; both are required and must be nonnegative.

!syntax parameters /Materials/KokkosPureElasticTractionSeparation

!syntax inputs /Materials/KokkosPureElasticTractionSeparation

!syntax children /Materials/KokkosPureElasticTractionSeparation
