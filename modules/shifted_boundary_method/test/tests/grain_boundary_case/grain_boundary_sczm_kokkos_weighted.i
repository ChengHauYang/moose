# The interface manager reads the true interfaces from one complete boundary mesh file
!include grain_boundary_sczm_kokkos_weighted_common.i

[Physics/SolidMechanics/ShiftedCohesiveZone/czm]
  complete_interface_mesh_file = 'grain_boundary_only.msh'
[]
