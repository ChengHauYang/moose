# ADCompositeElasticityTensor

`ADCompositeElasticityTensor` computes the weighted elasticity tensor described by
[CompositeElasticityTensor](CompositeElasticityTensor.md) from AD elasticity tensor and weight
properties. It preserves derivatives of both inputs through AD stress materials and mechanics
residuals.

Use AD producers for every property listed in
[!param](/Materials/ADCompositeElasticityTensor/tensors) and
[!param](/Materials/ADCompositeElasticityTensor/weights).

!syntax parameters /Materials/ADCompositeElasticityTensor

!syntax inputs /Materials/ADCompositeElasticityTensor

!syntax children /Materials/ADCompositeElasticityTensor
