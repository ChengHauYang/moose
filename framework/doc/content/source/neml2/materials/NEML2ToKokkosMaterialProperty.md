# NEML2ToKokkosMaterialProperty

!if! function=hasCapability('neml2')

!alert note
Users are not expected to directly use these objects in an input file. Instead, use the [NEML2 action](syntax/NEML2/index.md).

## Description

This family of objects transfers a NEML2 scalar, tensor output, or derivative to a Kokkos material property. The NEML2 executor moves the batched quantity from its compute device to the device used by Kokkos, if necessary, and, for symmetric tensors, the Kokkos material converts it from Mandel notation when evaluated.

The `Real` objects map a scalar NEML2 output to a `MaterialProperty<Real>`, matching the host [NEML2ToMOOSEMaterialProperty](NEML2ToMOOSEMaterialProperty.md) `Real` retriever. A scalar maps a model output only; it has no Mandel conversion and cannot map an input or parameter derivative.

The following objects are available:

| Class                                           | Kokkos material property rank | Writer            |
| :---------------------------------------------- | :----------------------------- | :----------------- |
| `NEML2ToKokkosRealMaterialProperty`             | Scalar                         | volume `Material`   |
| `NEML2ToKokkosRankTwoMaterialProperty`          | Two                            | volume `Material`   |
| `NEML2ToKokkosRankFourMaterialProperty`         | Four                           | volume `Material`   |
| `NEML2ToKokkosSideRealMaterialProperty`         | Scalar                         | `InterfaceMaterial`, primary side |
| `NEML2ToKokkosSideRankTwoMaterialProperty`      | Two                            | `InterfaceMaterial`, primary side |
| `NEML2ToKokkosSideRankFourMaterialProperty`     | Four                           | `InterfaceMaterial`, primary side |
| `NEML2ToKokkosNeighborRealMaterialProperty`     | Scalar                         | `InterfaceMaterial`, neighbor side |
| `NEML2ToKokkosNeighborRankTwoMaterialProperty`  | Two                            | `InterfaceMaterial`, neighbor side |
| `NEML2ToKokkosNeighborRankFourMaterialProperty` | Four                           | `InterfaceMaterial`, neighbor side |

The scalar objects expect an empty trailing NEML2 base shape. The rank-two objects expect a trailing NEML2 base shape of `[6]`. The rank-four objects expect `[6, 6]`. The supported Mandel component order is `xx`, `yy`, `zz`, `yz`, `xz`, `xy`.

The `Side` and `Neighbor` objects write an `InterfaceMaterial` property instead of a volume `Material` property, for use with `interface_only = true` on the [NEML2 action](syntax/NEML2/index.md). In both cases the NEML2 batch index for a given interface face is selected by the primary-side quadrature point (`primary_qp`): `MOOSEQuantityToNEML2` gathers neighbor-side input while traversing the primary quadrature loop, so the batch entry is always keyed by `primary_qp`. A `Side` object writes its output back at that same `primary_qp` in the element material storage. A `Neighbor` object instead writes to `neighbor_storage_qp`, the quadrature point in the neighbor material storage that corresponds to `primary_qp` across the face. The map from `primary_qp` to `neighbor_storage_qp` must be a bijection.

!syntax parameters /Materials/NEML2ToKokkosRankTwoMaterialProperty

!if-end!

!else

!include neml2/neml2_warning.md
