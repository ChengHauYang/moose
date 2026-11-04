# Independent check of the element-volume average M used by the total-mode F-bar interface.
#
# A CONSTANT MONOMIAL auxiliary variable of a physical deformation gradient component U_ij is the
# L2 projection of U_ij onto constants, which is its average over the element volume quadrature.
# On both sides of the interface, the face average of M_ij must therefore equal the face average of
# that auxiliary variable, while the face average of U_ij itself differs from it on this non-affine
# mesh, and the F-bar deformation gradient B differs from U.

!include sczm_tl_fbar_host.i

[AuxVariables]
  [U00]
    family = MONOMIAL
    order = CONSTANT
  []
  [U11]
    family = MONOMIAL
    order = CONSTANT
  []
  [U22]
    family = MONOMIAL
    order = CONSTANT
  []
  [U01]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  [U00]
    type = MaterialRankTwoTensorAux
    variable = U00
    property = physical_deformation_gradient
    i = 0
    j = 0
    execute_on = TIMESTEP_END
  []
  [U11]
    type = MaterialRankTwoTensorAux
    variable = U11
    property = physical_deformation_gradient
    i = 1
    j = 1
    execute_on = TIMESTEP_END
  []
  [U22]
    type = MaterialRankTwoTensorAux
    variable = U22
    property = physical_deformation_gradient
    i = 2
    j = 2
    execute_on = TIMESTEP_END
  []
  [U01]
    type = MaterialRankTwoTensorAux
    variable = U01
    property = physical_deformation_gradient
    i = 0
    j = 1
    execute_on = TIMESTEP_END
  []
[]

[Postprocessors]
  [M00_elem]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '0 0'
    boundary = Block0_Block1
  []
  [U00_volume_elem]
    type = SideAverageValue
    variable = U00
    boundary = Block0_Block1
  []
  [M11_elem]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '1 1'
    boundary = Block0_Block1
  []
  [U11_volume_elem]
    type = SideAverageValue
    variable = U11
    boundary = Block0_Block1
  []
  [M22_elem]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '2 2'
    boundary = Block0_Block1
  []
  [U22_volume_elem]
    type = SideAverageValue
    variable = U22
    boundary = Block0_Block1
  []
  [M01_elem]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '0 1'
    boundary = Block0_Block1
  []
  [U01_volume_elem]
    type = SideAverageValue
    variable = U01
    boundary = Block0_Block1
  []
  [U00_face_elem]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '0 0'
    boundary = Block0_Block1
  []
  [B00_face_elem]
    type = SideAverageMaterialProperty
    property = deformation_gradient
    component = '0 0'
    boundary = Block0_Block1
  []
  [M00_neighbor]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '0 0'
    boundary = Block1_Block0
  []
  [U00_volume_neighbor]
    type = SideAverageValue
    variable = U00
    boundary = Block1_Block0
  []
  [M11_neighbor]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '1 1'
    boundary = Block1_Block0
  []
  [U11_volume_neighbor]
    type = SideAverageValue
    variable = U11
    boundary = Block1_Block0
  []
  [M22_neighbor]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '2 2'
    boundary = Block1_Block0
  []
  [U22_volume_neighbor]
    type = SideAverageValue
    variable = U22
    boundary = Block1_Block0
  []
  [M01_neighbor]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '0 1'
    boundary = Block1_Block0
  []
  [U01_volume_neighbor]
    type = SideAverageValue
    variable = U01
    boundary = Block1_Block0
  []
  [U00_face_neighbor]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '0 0'
    boundary = Block1_Block0
  []
  [B00_face_neighbor]
    type = SideAverageMaterialProperty
    property = deformation_gradient
    component = '0 0'
    boundary = Block1_Block0
  []
[]

[UserObjects]
  # M and the auxiliary volume average are both constant on each element and are integrated with the
  # same face weights, so they agree to roundoff; 1e-12 is far above roundoff for O(1) values.
  [average_is_volume_based]
    type = Terminator
    expression = 'abs(M00_elem - U00_volume_elem) > 1e-12 | abs(M11_elem - U11_volume_elem) > 1e-12 | abs(M22_elem - U22_volume_elem) > 1e-12 | abs(M01_elem - U01_volume_elem) > 1e-12 | abs(M00_neighbor - U00_volume_neighbor) > 1e-12 | abs(M11_neighbor - U11_volume_neighbor) > 1e-12 | abs(M22_neighbor - U22_volume_neighbor) > 1e-12 | abs(M01_neighbor - U01_volume_neighbor) > 1e-12'
    fail_mode = HARD
    error_level = ERROR
    message = 'The interface M does not equal the element-volume average of U.'
    execution_order_group = 1
  []
  # Face and volume averages of U differ here by about 3e-5, and B and U by about 2e-4; 1e-6 rules
  # out a vacuous check without depending on the exact values.
  [kinematics_are_nontrivial]
    type = Terminator
    expression = 'abs(U00_face_elem - U00_volume_elem) < 1e-6 | abs(B00_face_elem - U00_face_elem) < 1e-6 | abs(U00_face_neighbor - U00_volume_neighbor) < 1e-6 | abs(B00_face_neighbor - U00_face_neighbor) < 1e-6'
    fail_mode = HARD
    error_level = ERROR
    message = 'The test does not distinguish face from volume averages or B from U.'
    execution_order_group = 1
  []
[]
