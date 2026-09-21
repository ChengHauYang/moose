# KokkosQuantityToNEML2

!if! function=hasCapability('neml2')

!alert note
Users are +NOT+ expected to directly use this object in an input file. Instead, it is always recommended to use the [NEML2 action](syntax/NEML2/index.md).

## Description

This object collects a MOOSE variable given by [!param](/UserObjects/KokkosRealToNEML2/from_moose) for use as a NEML2 input variable [!param](/UserObjects/KokkosRealToNEML2/to_neml2), directly on the device used by the [Kokkos output backend](syntax/NEML2/index.md) without round-tripping the gathered field through the host. It is created by the NEML2 action when the output backend is Kokkos and a `Real` or `RealVectorValue` VARIABLE input (current, old, or older state) is gathered; all other inputs (host scalars, functions, etc.) are gathered by [MOOSEQuantityToNEML2](MOOSEQuantityToNEML2.md) on the host.

!syntax parameters /UserObjects/KokkosRealToNEML2

!if-end!

!else

!include neml2/neml2_warning.md