# ADComplianceSensitivity

!syntax description /Materials/ADComplianceSensitivity

## Description

`ADComplianceSensitivity` computes compliance sensitivity while preserving automatic derivatives from the current stress, strain, Young's modulus, and its derivative with respect to the design density. The Young's modulus and its named derivative must be supplied by an AD material such as `ADDerivativeParsedMaterial`.

The non-AD `ComplianceSensitivity` object remains available for existing inputs.

!syntax parameters /Materials/ADComplianceSensitivity

!syntax inputs /Materials/ADComplianceSensitivity

!syntax children /Materials/ADComplianceSensitivity
