# NEML2ToKokkosMaterialProperty

!if! function=hasCapability('neml2')

!alert note
Users are not expected to directly use these objects in an input file. Instead, use the [NEML2 action](syntax/NEML2/index.md).

## Description

This family of objects transfers a NEML2 symmetric tensor output or derivative to a Kokkos material property. The NEML2 executor moves the batched tensor from its compute device to the device used by Kokkos, if necessary, and the Kokkos material converts it from Mandel notation when evaluated.

The following objects are available:

| Class                                           | Kokkos material property rank | Writer            |
| :---------------------------------------------- | :----------------------------- | :----------------- |
| `NEML2ToKokkosRankTwoMaterialProperty`          | Two                            | volume `Material`   |
| `NEML2ToKokkosRankFourMaterialProperty`         | Four                           | volume `Material`   |
| `NEML2ToKokkosSideRankTwoMaterialProperty`      | Two                            | `InterfaceMaterial`, primary side |
| `NEML2ToKokkosSideRankFourMaterialProperty`     | Four                           | `InterfaceMaterial`, primary side |
| `NEML2ToKokkosNeighborRankTwoMaterialProperty`  | Two                            | `InterfaceMaterial`, neighbor side |
| `NEML2ToKokkosNeighborRankFourMaterialProperty` | Four                           | `InterfaceMaterial`, neighbor side |

The rank-two objects expect a trailing NEML2 base shape of `[6]`. The rank-four objects expect `[6, 6]`. The supported Mandel component order is `xx`, `yy`, `zz`, `yz`, `xz`, `xy`.

The `Side` and `Neighbor` objects write an `InterfaceMaterial` property instead of a volume `Material` property, for use with `interface_only = true` on the [NEML2 action](syntax/NEML2/index.md). In both cases the NEML2 batch index for a given interface face is selected by the primary-side quadrature point (`primary_qp`): `MOOSEQuantityToNEML2` gathers neighbor-side input while traversing the primary quadrature loop, so the batch entry is always keyed by `primary_qp`. A `Side` object writes its output back at that same `primary_qp` in the element material storage. A `Neighbor` object instead writes to `neighbor_storage_qp`, the quadrature point in the neighbor material storage that corresponds to `primary_qp` across the face. The map from `primary_qp` to `neighbor_storage_qp` must be a bijection.

!syntax parameters /Materials/NEML2ToKokkosRankTwoMaterialProperty

!if-end!

!else

!include neml2/neml2_warning.md
