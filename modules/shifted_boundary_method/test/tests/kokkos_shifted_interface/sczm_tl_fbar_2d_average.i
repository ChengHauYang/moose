# Plane-strain check of the total-mode F-bar interface kinematics against the host bulk
# total-Lagrangian strain calculator (base_name = bulk), which is the reference formulation.
#
# On both sides of the interface:
#   * U on the interface equals the bulk unstabilized deformation gradient, with U_zz = 1.
#   * M on the interface equals the bulk element average, read from a CONSTANT MONOMIAL auxiliary
#     variable. The bulk M is constant on each element because it is computed in the element volume.
#   * B equals the bulk F-bar deformation gradient in the element volume. A CONSTANT MONOMIAL
#     auxiliary variable is the L2 projection of a property onto constants, i.e. its element-volume
#     average, so equal auxiliary values compare B over the whole element. B_zz = gamma is the
#     cube-root volume ratio.
# The bulk B is not compared on the interface itself: there the bulk calculator averages over the
# face quadrature points rather than over the element volume.

!include sczm_tl_fbar_2d_host.i

[AuxVariables]
  [bulk_M00]
    family = MONOMIAL
    order = CONSTANT
  []
  [bulk_M01]
    family = MONOMIAL
    order = CONSTANT
  []
  [bulk_M11]
    family = MONOMIAL
    order = CONSTANT
  []
  [B00]
    family = MONOMIAL
    order = CONSTANT
  []
  [bulk_B00]
    family = MONOMIAL
    order = CONSTANT
  []
  [B01]
    family = MONOMIAL
    order = CONSTANT
  []
  [bulk_B01]
    family = MONOMIAL
    order = CONSTANT
  []
  [B22]
    family = MONOMIAL
    order = CONSTANT
  []
  [bulk_B22]
    family = MONOMIAL
    order = CONSTANT
  []
[]

[AuxKernels]
  [bulk_M00]
    type = MaterialRankTwoTensorAux
    variable = bulk_M00
    property = bulk_average_deformation_gradient
    i = 0
    j = 0
    execute_on = TIMESTEP_END
  []
  [bulk_M01]
    type = MaterialRankTwoTensorAux
    variable = bulk_M01
    property = bulk_average_deformation_gradient
    i = 0
    j = 1
    execute_on = TIMESTEP_END
  []
  [bulk_M11]
    type = MaterialRankTwoTensorAux
    variable = bulk_M11
    property = bulk_average_deformation_gradient
    i = 1
    j = 1
    execute_on = TIMESTEP_END
  []
  [B00]
    type = MaterialRankTwoTensorAux
    variable = B00
    property = deformation_gradient
    i = 0
    j = 0
    execute_on = TIMESTEP_END
  []
  [bulk_B00]
    type = MaterialRankTwoTensorAux
    variable = bulk_B00
    property = bulk_deformation_gradient
    i = 0
    j = 0
    execute_on = TIMESTEP_END
  []
  [B01]
    type = MaterialRankTwoTensorAux
    variable = B01
    property = deformation_gradient
    i = 0
    j = 1
    execute_on = TIMESTEP_END
  []
  [bulk_B01]
    type = MaterialRankTwoTensorAux
    variable = bulk_B01
    property = bulk_deformation_gradient
    i = 0
    j = 1
    execute_on = TIMESTEP_END
  []
  [B22]
    type = MaterialRankTwoTensorAux
    variable = B22
    property = deformation_gradient
    i = 2
    j = 2
    execute_on = TIMESTEP_END
  []
  [bulk_B22]
    type = MaterialRankTwoTensorAux
    variable = bulk_B22
    property = bulk_deformation_gradient
    i = 2
    j = 2
    execute_on = TIMESTEP_END
  []
[]

[Postprocessors]
  [U01_elem]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '0 1'
    boundary = Block0_Block1
  []
  [bulk_U01_elem]
    type = SideAverageMaterialProperty
    property = bulk_unstabilized_deformation_gradient
    component = '0 1'
    boundary = Block0_Block1
  []
  [U10_elem]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '1 0'
    boundary = Block0_Block1
  []
  [bulk_U10_elem]
    type = SideAverageMaterialProperty
    property = bulk_unstabilized_deformation_gradient
    component = '1 0'
    boundary = Block0_Block1
  []
  [U22_elem]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '2 2'
    boundary = Block0_Block1
  []
  [U00_elem]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '0 0'
    boundary = Block0_Block1
  []
  [M00_elem]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '0 0'
    boundary = Block0_Block1
  []
  [bulk_M00_elem]
    type = SideAverageValue
    variable = bulk_M00
    boundary = Block0_Block1
  []
  [M01_elem]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '0 1'
    boundary = Block0_Block1
  []
  [bulk_M01_elem]
    type = SideAverageValue
    variable = bulk_M01
    boundary = Block0_Block1
  []
  [M11_elem]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '1 1'
    boundary = Block0_Block1
  []
  [bulk_M11_elem]
    type = SideAverageValue
    variable = bulk_M11
    boundary = Block0_Block1
  []
  [M22_elem]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '2 2'
    boundary = Block0_Block1
  []
  [B00_elem]
    type = SideAverageValue
    variable = B00
    boundary = Block0_Block1
  []
  [bulk_B00_elem]
    type = SideAverageValue
    variable = bulk_B00
    boundary = Block0_Block1
  []
  [B01_elem]
    type = SideAverageValue
    variable = B01
    boundary = Block0_Block1
  []
  [bulk_B01_elem]
    type = SideAverageValue
    variable = bulk_B01
    boundary = Block0_Block1
  []
  [B22_elem]
    type = SideAverageValue
    variable = B22
    boundary = Block0_Block1
  []
  [bulk_B22_elem]
    type = SideAverageValue
    variable = bulk_B22
    boundary = Block0_Block1
  []

  [U01_neighbor]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '0 1'
    boundary = Block1_Block0
  []
  [bulk_U01_neighbor]
    type = SideAverageMaterialProperty
    property = bulk_unstabilized_deformation_gradient
    component = '0 1'
    boundary = Block1_Block0
  []
  [U10_neighbor]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '1 0'
    boundary = Block1_Block0
  []
  [bulk_U10_neighbor]
    type = SideAverageMaterialProperty
    property = bulk_unstabilized_deformation_gradient
    component = '1 0'
    boundary = Block1_Block0
  []
  [U22_neighbor]
    type = SideAverageMaterialProperty
    property = physical_deformation_gradient
    component = '2 2'
    boundary = Block1_Block0
  []
  [M00_neighbor]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '0 0'
    boundary = Block1_Block0
  []
  [bulk_M00_neighbor]
    type = SideAverageValue
    variable = bulk_M00
    boundary = Block1_Block0
  []
  [M01_neighbor]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '0 1'
    boundary = Block1_Block0
  []
  [bulk_M01_neighbor]
    type = SideAverageValue
    variable = bulk_M01
    boundary = Block1_Block0
  []
  [M11_neighbor]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '1 1'
    boundary = Block1_Block0
  []
  [bulk_M11_neighbor]
    type = SideAverageValue
    variable = bulk_M11
    boundary = Block1_Block0
  []
  [M22_neighbor]
    type = SideAverageMaterialProperty
    property = average_deformation_gradient
    component = '2 2'
    boundary = Block1_Block0
  []
  [B00_neighbor]
    type = SideAverageValue
    variable = B00
    boundary = Block1_Block0
  []
  [bulk_B00_neighbor]
    type = SideAverageValue
    variable = bulk_B00
    boundary = Block1_Block0
  []
  [B01_neighbor]
    type = SideAverageValue
    variable = B01
    boundary = Block1_Block0
  []
  [bulk_B01_neighbor]
    type = SideAverageValue
    variable = bulk_B01
    boundary = Block1_Block0
  []
  [B22_face_elem]
    type = SideAverageMaterialProperty
    property = deformation_gradient
    component = '2 2'
    boundary = Block0_Block1
  []
  [B22_face_neighbor]
    type = SideAverageMaterialProperty
    property = deformation_gradient
    component = '2 2'
    boundary = Block1_Block0
  []
  [B22_neighbor]
    type = SideAverageValue
    variable = B22
    boundary = Block1_Block0
  []
  [bulk_B22_neighbor]
    type = SideAverageValue
    variable = bulk_B22
    boundary = Block1_Block0
  []
[]

[UserObjects]
  # Each pair evaluates the same quantity from the same displacements with the same quadrature, so
  # the pairs agree to roundoff; 1e-12 is far above roundoff for O(1) values.
  [interface_matches_bulk]
    type = Terminator
    expression = 'abs(U01_elem - bulk_U01_elem) > 1e-12 | abs(U10_elem - bulk_U10_elem) > 1e-12 | abs(U22_elem - 1) > 1e-12 | abs(M00_elem - bulk_M00_elem) > 1e-12 | abs(M01_elem - bulk_M01_elem) > 1e-12 | abs(M11_elem - bulk_M11_elem) > 1e-12 | abs(M22_elem - 1) > 1e-12 | abs(B00_elem - bulk_B00_elem) > 1e-12 | abs(B01_elem - bulk_B01_elem) > 1e-12 | abs(B22_elem - bulk_B22_elem) > 1e-12 | abs(U01_neighbor - bulk_U01_neighbor) > 1e-12 | abs(U10_neighbor - bulk_U10_neighbor) > 1e-12 | abs(U22_neighbor - 1) > 1e-12 | abs(M00_neighbor - bulk_M00_neighbor) > 1e-12 | abs(M01_neighbor - bulk_M01_neighbor) > 1e-12 | abs(M11_neighbor - bulk_M11_neighbor) > 1e-12 | abs(M22_neighbor - 1) > 1e-12 | abs(B00_neighbor - bulk_B00_neighbor) > 1e-12 | abs(B01_neighbor - bulk_B01_neighbor) > 1e-12 | abs(B22_neighbor - bulk_B22_neighbor) > 1e-12'
    fail_mode = HARD
    error_level = ERROR
    message = 'The plane-strain interface U, M, or B differs from the host bulk F-bar kinematics.'
    execution_order_group = 1
  []
  # Rules out a vacuous check: the face average of U must differ from the element average M, and
  # B_zz = gamma must differ from one on the interface. These differ by about 6e-5 and 2e-5 to
  # 1.3e-4 here; 1e-6 rules out a vacuous check without depending on the exact values. The element
  # average of gamma is much closer to one because its first-order variation averages out.
  [kinematics_are_nontrivial]
    type = Terminator
    expression = 'abs(U00_elem - M00_elem) < 1e-6 | abs(B22_face_elem - 1) < 1e-6 | abs(B22_face_neighbor - 1) < 1e-6'
    fail_mode = HARD
    error_level = ERROR
    message = 'The test does not distinguish face from volume averages or B from U.'
    execution_order_group = 1
  []
[]
