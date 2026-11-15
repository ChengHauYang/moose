# KokkosQuantityToNEML2

!syntax description /UserObjects/KokkosQuantityToNEML2

## Description

`KokkosQuantityToNEML2` gathers a current-state full rank-two Kokkos material property into an
NEML2 input tensor for bulk elements. The [!param](/UserObjects/KokkosQuantityToNEML2/from_moose)
parameter names the MOOSE property and [!param](/UserObjects/KokkosQuantityToNEML2/to_neml2)
names the NEML2 input variable. The
[!param](/UserObjects/KokkosQuantityToNEML2/batch_index_generator) parameter supplies the
batch-row destinations.

This object is created automatically by [NEML2Action](../actions/NEML2Action.md) when
[!param](/NEML2/NEML2Action/input_backend) is `kokkos` for a bulk NEML2 model.

## Example Input Syntax

In this example, the NEML2 Action gathers the Kokkos material property `F` into the NEML2 input
of the same name.

!listing test/tests/neml2/kokkos_quantity_to_neml2_bulk.i block=NEML2

!syntax parameters /UserObjects/KokkosQuantityToNEML2

!syntax inputs /UserObjects/KokkosQuantityToNEML2

!syntax children /UserObjects/KokkosQuantityToNEML2
