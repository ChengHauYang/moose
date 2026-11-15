# SCZMComputeDisplacementJumpTotalLagrangian

!syntax description /Materials/SCZMComputeDisplacementJumpTotalLagrangian

This material Taylor-shifts the displacement jump from the surrogate interface to the true
interface and transforms it to the local cohesive frame using total-Lagrangian kinematics. The
reference frame uses the true-interface normal. The deformation gradient is averaged across the
surrogate interface and is not Taylor-shifted to the true interface.

!syntax parameters /Materials/SCZMComputeDisplacementJumpTotalLagrangian

!syntax inputs /Materials/SCZMComputeDisplacementJumpTotalLagrangian

!syntax children /Materials/SCZMComputeDisplacementJumpTotalLagrangian
