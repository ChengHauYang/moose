# GlobalStrain / ADGlobalStrain

!syntax description /ScalarKernels/ADGlobalStrain

## Description

`ADGlobalStrain` solves the scalar equation for global strain while preserving derivatives through AD. It obtains the AD residual from `ADGlobalStrainUserObject` and lets MOOSE assemble the resulting scalar and coupled Jacobian blocks automatically.

!syntax parameters /ScalarKernels/ADGlobalStrain

!syntax inputs /ScalarKernels/ADGlobalStrain

!syntax children /ScalarKernels/ADGlobalStrain
