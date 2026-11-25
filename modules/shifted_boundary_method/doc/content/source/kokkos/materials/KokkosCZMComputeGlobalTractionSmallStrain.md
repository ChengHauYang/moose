# KokkosCZMComputeGlobalTractionSmallStrain

!syntax description /Materials/KokkosCZMComputeGlobalTractionSmallStrain

This interface material rotates the local cohesive traction and its tangent into global coordinates
for small-strain kinematics on the Kokkos backend. It reads `czm_total_rotation`,
`interface_traction`, and `dinterface_traction_djump` and declares `traction_global` and
`dtraction_djump_global`, all prefixed by `base_name`. The global properties are consumed by
[KokkosSCZMInterfaceKernelSmallStrain](KokkosSCZMInterfaceKernelSmallStrain.md).

!syntax parameters /Materials/KokkosCZMComputeGlobalTractionSmallStrain

!syntax inputs /Materials/KokkosCZMComputeGlobalTractionSmallStrain

!syntax children /Materials/KokkosCZMComputeGlobalTractionSmallStrain
