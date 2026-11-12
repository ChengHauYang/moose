# KokkosInterfaceQuantityToNEML2

!syntax description /UserObjects/KokkosInterfaceQuantityToNEML2

## Description

`KokkosInterfaceQuantityToNEML2` gathers current-state full rank-two Kokkos material properties
from the primary and neighbor sides of a cohesive interface into one NEML2 input tensor. The
[!param](/UserObjects/KokkosInterfaceQuantityToNEML2/primary_from_moose) and
[!param](/UserObjects/KokkosInterfaceQuantityToNEML2/neighbor_from_moose) parameters name the
properties on each side, while
[!param](/UserObjects/KokkosInterfaceQuantityToNEML2/batch_index_generator) provides their
batch-row destinations.

This object is created automatically by [NEML2Action](../actions/NEML2Action.md) when
[!param](/NEML2/NEML2Action/input_backend) is `kokkos` for an `interface_only` NEML2 model.

## Example Input Syntax

In this example, the NEML2 Action gathers the primary and neighbor Kokkos material properties
named `F` into one NEML2 input.

!listing test/tests/neml2/kokkos_interface_quantity_to_neml2.i block=NEML2

!syntax parameters /UserObjects/KokkosInterfaceQuantityToNEML2

!syntax inputs /UserObjects/KokkosInterfaceQuantityToNEML2

!syntax children /UserObjects/KokkosInterfaceQuantityToNEML2
