# ADCompositeEigenstrain

`ADCompositeEigenstrain` computes the weighted eigenstrain described by
[CompositeEigenstrain](CompositeEigenstrain.md) from AD tensor and weight properties. It preserves
derivatives of both inputs through the strain and stress material chain.

Use AD producers for every property listed in [!param](/Materials/ADCompositeEigenstrain/tensors) and
[!param](/Materials/ADCompositeEigenstrain/weights).

!syntax parameters /Materials/ADCompositeEigenstrain

!syntax inputs /Materials/ADCompositeEigenstrain

!syntax children /Materials/ADCompositeEigenstrain
