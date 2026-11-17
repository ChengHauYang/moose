# Heterogeneous cohesive interface between an elastic block 0 (primary side of Block0_Block1) and a
# stateful J2 radial-return block 1 (neighbor side). Each [NEML2] sub-block is restricted to its own
# block and evaluates the interface only on its side, so the stateful model sees only block-1 strains
# and keeps its history only on the neighbor side. The loading of sczm_stateful_common.i yields
# block 1 and then unloads it.

!include sczm_stateful_kokkos.i

[NEML2]
  [constitutive]
    block = 1
  []
  [elastic]
    input = sczm_hetero_plastic_neml2.i
    model = elastic_interface_model
    block = 0
    interface = Block0_Block1
    volume = false
    executor_name = neml2_elastic
    output_variables = sczm_stress
    output_backend = kokkos
  []
[]
