# KokkosSCZMComputeDisplacementJumpSmallStrain

!syntax description /Materials/KokkosSCZMComputeDisplacementJumpSmallStrain

This interface material is the Kokkos counterpart of
[SCZMComputeDisplacementJumpSmallStrain](SCZMComputeDisplacementJumpSmallStrain.md). It computes
the shifted displacement jump in global coordinates (`displacement_jump_global`) and in the local
interface frame (`interface_displacement_jump`), together with the rotation `czm_total_rotation`,
all prefixed by `base_name`. The surrogate-to-true distance vectors and rotations are taken from
the surrogate-interface mapping of `sbm_distance_uo`. With `no_shifted = true`, the jump omits the
displacement-gradient contribution and the reference rotation is built from the surrogate face
normal instead of the true normal.

!syntax parameters /Materials/KokkosSCZMComputeDisplacementJumpSmallStrain

!syntax inputs /Materials/KokkosSCZMComputeDisplacementJumpSmallStrain

!syntax children /Materials/KokkosSCZMComputeDisplacementJumpSmallStrain
