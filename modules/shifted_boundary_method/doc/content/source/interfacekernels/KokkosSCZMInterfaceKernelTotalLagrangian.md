# KokkosSCZMInterfaceKernelTotalLagrangian

!syntax description /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

This interface kernel assembles cohesive PK1 traction on a surrogate interface using Kokkos for
total-Lagrangian finite-strain kinematics. It applies the true-to-surrogate reference-area projection
and the directional correction based on bulk PK1 stress. Its Jacobian includes derivatives through both
the shifted displacement jump and the surrogate-interface deformation gradient.

!syntax parameters /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

!syntax inputs /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian

!syntax children /InterfaceKernels/KokkosSCZMInterfaceKernelTotalLagrangian
