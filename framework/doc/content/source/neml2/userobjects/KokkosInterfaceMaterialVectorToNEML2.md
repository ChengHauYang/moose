# KokkosInterfaceMaterialVectorToNEML2

!syntax description /UserObjects/KokkosInterfaceMaterialVectorToNEML2

## Description

`KokkosInterfaceMaterialVectorToNEML2` gathers a current-state vector Kokkos interface material
property, which holds one value per interface quadrature point, into a NEML2 input tensor. It is
the Kokkos counterpart of a host [MOOSEQuantityToNEML2](MOOSEQuantityToNEML2.md) gathering
interface material data, and lets a NEML2 model act as the traction-separation law of a Kokkos
cohesive zone: the displacement jump computed by a Kokkos interface material is gathered into
NEML2, and the traction and its derivative with respect to the jump are returned through
[NEML2ToKokkosMaterialProperty](NEML2ToKokkosMaterialProperty.md) interface materials.

The value at each interface quadrature point is written to the batch row of the primary side and,
if the neighbor side is also batched, to the row of the neighbor side, so both evaluate the model
at the same interface value.
[!param](/UserObjects/KokkosInterfaceMaterialVectorToNEML2/batch_index_generator) provides the
batch-row destinations.

This object is created automatically by [NEML2Action](../actions/NEML2Action.md) for every input
listed in [!param](/NEML2/NEML2Action/interface_material_inputs) when
[!param](/NEML2/NEML2Action/input_backend) is `kokkos` for an interface-only NEML2 model
(`volume = false` with only `interface` set).

## Example Input Syntax

In this example, a NEML2 linear traction-separation law replaces a Kokkos one in a shifted
cohesive zone.

!listing modules/shifted_boundary_method/test/tests/kokkos_shifted_interface/sczm_tl_3d_neml2_traction.i block=NEML2

!syntax parameters /UserObjects/KokkosInterfaceMaterialVectorToNEML2

!syntax inputs /UserObjects/KokkosInterfaceMaterialVectorToNEML2

!syntax children /UserObjects/KokkosInterfaceMaterialVectorToNEML2
