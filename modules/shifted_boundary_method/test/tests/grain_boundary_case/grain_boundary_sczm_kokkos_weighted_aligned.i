# The true interfaces come from one surface mesh per interface, without the interface manager
!include grain_boundary_sczm_kokkos_weighted_common.i

[Mesh]
  [grain1_grain2]
    type = FileMeshGenerator
    file = 'grain1_grain2.msh'
    save_with_name = 'grain1_grain2'
  []
  [grain1_grain4]
    type = FileMeshGenerator
    file = 'grain1_grain4.msh'
    save_with_name = 'grain1_grain4'
  []
  [grain1_grain5]
    type = FileMeshGenerator
    file = 'grain1_grain5.msh'
    save_with_name = 'grain1_grain5'
  []
  [grain2_grain4]
    type = FileMeshGenerator
    file = 'grain2_grain4.msh'
    save_with_name = 'grain2_grain4'
  []
  [grain3_grain4]
    type = FileMeshGenerator
    file = 'grain3_grain4.msh'
    save_with_name = 'grain3_grain4'
  []
  [grain3_grain5]
    type = FileMeshGenerator
    file = 'grain3_grain5.msh'
    save_with_name = 'grain3_grain5'
  []
  [grain4_grain5]
    type = FileMeshGenerator
    file = 'grain4_grain5.msh'
    save_with_name = 'grain4_grain5'
  []
[]

[Physics/SolidMechanics/ShiftedCohesiveZone/czm]
  generate_sbm_distance = true
  surface_meshes = ${interfaces}
[]
