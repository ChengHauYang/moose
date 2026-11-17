# ADInclusionProperties

`ADInclusionProperties` provides AD stress, strain, and elastic-energy properties for the analytical
elliptical inclusion solution described by [InclusionProperties](InclusionProperties.md).
Its inputs are prescribed geometry, elastic constants, and misfit strains evaluated on the
undisplaced mesh, so its outputs have zero nonlinear-variable derivatives by design.

In this example, automatic material-property output uses AD-aware auxiliary kernels. Every stress
and strain component and the elastic energy are compared with the non-AD solution on a mesh
spanning the interior and exterior of an elliptical inclusion. This checks value and property-type
compatibility.

!listing modules/solid_mechanics/test/tests/ad_migration/inclusion_properties.i block=Materials

!syntax parameters /Materials/ADInclusionProperties

!syntax inputs /Materials/ADInclusionProperties

!syntax children /Materials/ADInclusionProperties
