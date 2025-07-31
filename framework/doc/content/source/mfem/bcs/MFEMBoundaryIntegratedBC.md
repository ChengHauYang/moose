<<<<<<<< HEAD:framework/doc/content/source/mfem/bcs/MFEMScalarBoundaryIntegratedBC.md
# MFEMScalarBoundaryIntegratedBC
========
# MFEMBoundaryIntegratedBC
>>>>>>>> upstream/next:framework/doc/content/source/mfem/bcs/MFEMBoundaryIntegratedBC.md

!if! function=hasCapability('mfem')

## Summary

<<<<<<<< HEAD:framework/doc/content/source/mfem/bcs/MFEMScalarBoundaryIntegratedBC.md
!syntax description /BCs/MFEMScalarBoundaryIntegratedBC
========
!syntax description /BCs/MFEMBoundaryIntegratedBC
>>>>>>>> upstream/next:framework/doc/content/source/mfem/bcs/MFEMBoundaryIntegratedBC.md

## Overview

Adds the boundary integrator for integrating the linear form

!equation
(f, v)_{\partial\Omega} \,\,\, \forall v \in V

where the test variable $v \in H^1$ and $f$ is a scalar coefficient. Often used for representing
Neumann-type boundary conditions.

<<<<<<<< HEAD:framework/doc/content/source/mfem/bcs/MFEMScalarBoundaryIntegratedBC.md
!syntax parameters /BCs/MFEMScalarBoundaryIntegratedBC

!syntax inputs /BCs/MFEMScalarBoundaryIntegratedBC

!syntax children /BCs/MFEMScalarBoundaryIntegratedBC
========
!syntax parameters /BCs/MFEMBoundaryIntegratedBC

!syntax inputs /BCs/MFEMBoundaryIntegratedBC

!syntax children /BCs/MFEMBoundaryIntegratedBC
>>>>>>>> upstream/next:framework/doc/content/source/mfem/bcs/MFEMBoundaryIntegratedBC.md

!if-end!

!else
!include mfem/mfem_warning.md
