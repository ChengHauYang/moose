[Models]
  [A]
    type = R2ConstantParameter
    value = A_tensor
    parameter = A_value
  []
  [B]
    type = R2ConstantParameter
    value = B_tensor
    parameter = B_value
  []
  [left]
    type = R2Multiplication
    A = A_value
    B = deformation_gradient
    to = left_product
  []
  [right]
    type = R2Multiplication
    A = left_product
    B = B_value
    to = pk2_stress
  []
  [model]
    type = ComposedModel
    models = 'A B left right'
  []
[]

[Tensors]
  [A_tensor]
    type = Python
    expr = 'R2(torch.tensor([[0.9, 0.1, 0.4], [0.2, 1.2, 0.3], [0.6, 0.5, 0.8]], dtype=torch.float64))'
  []
  [B_tensor]
    type = Python
    expr = 'R2(torch.tensor([[0.7, 0.3, 0.6], [0.2, 1.3, 0.4], [0.5, 0.1, 1.1]], dtype=torch.float64))'
  []
[]
