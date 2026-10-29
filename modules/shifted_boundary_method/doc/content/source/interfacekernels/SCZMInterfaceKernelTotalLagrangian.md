# SCZMInterfaceKernelTotalLagrangian

!syntax description /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

This interface kernel assembles cohesive PK1 traction on a surrogate interface. It applies the
true-to-surrogate reference-area projection and the directional correction based on bulk PK1
stress. Its non-AD Jacobian includes derivatives through both the shifted displacement jump and the
surrogate-interface deformation gradient.

!syntax parameters /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

!syntax inputs /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian

!syntax children /InterfaceKernels/SCZMInterfaceKernelTotalLagrangian
