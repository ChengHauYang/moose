# KokkosElementExtremeMaterialProperty

!if! function=hasCapability('kokkos')

This is the Kokkos version of [ElementExtremeMaterialProperty](ElementExtremeMaterialProperty.md). See the original document for details.

## Example Input Syntax

!listing test/tests/neml2/kokkos_scalar_output.i block=Postprocessors

!syntax parameters /Postprocessors/KokkosElementExtremeMaterialProperty

!syntax inputs /Postprocessors/KokkosElementExtremeMaterialProperty

!syntax children /Postprocessors/KokkosElementExtremeMaterialProperty

!if-end!

!else
!include kokkos/kokkos_warning.md
