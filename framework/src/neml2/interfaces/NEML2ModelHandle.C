//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NEML2ModelHandle.h"

#ifdef NEML2_ENABLED
#include <ATen/ExpandUtils.h>

namespace
{
// AOTI's compiled IFT blocks use one dynamic batch axis. Base axes, including both
// axes of a full rank-two tensor, must remain intact.
at::Tensor
flattenNEML2Batch(const at::Tensor & tensor, const std::vector<int64_t> & base_shape)
{
  const auto base_ndim = static_cast<int64_t>(base_shape.size());
  TORCH_CHECK(tensor.dim() >= base_ndim &&
                  tensor.sizes().slice(tensor.dim() - base_ndim).equals(base_shape),
              "NEML2 tensor has shape ",
              tensor.sizes(),
              "; expected trailing base shape ",
              at::IntArrayRef(base_shape));
  const auto batch_ndim = tensor.dim() - base_ndim;
  return batch_ndim > 1 ? tensor.flatten(0, batch_ndim - 1) : tensor;
}

// Batch-independent outputs and derivative blocks retain their base-only shape.
void
restoreNEML2Batch(at::Tensor & tensor,
                  const std::vector<int64_t> & batch_shape,
                  const std::size_t base_ndim)
{
  if (batch_shape.size() <= 1 || !tensor.defined() ||
      tensor.dim() == static_cast<int64_t>(base_ndim))
    return;
  auto shape = batch_shape;
  const auto base = tensor.sizes().slice(1);
  shape.insert(shape.end(), base.begin(), base.end());
  tensor = tensor.reshape(shape);
}
}

std::map<std::string, at::Tensor>
AOTIModelHandle::flattenInputs(const std::map<std::string, at::Tensor> & inputs,
                               std::vector<int64_t> & batch_shape) const
{
  const auto & names = input_names();
  const auto & shapes = input_base_shapes();
  for (const auto i : index_range(names))
  {
    const auto & tensor = libmesh_map_find(inputs, names[i]);
    // Validate before interpreting the leading axes as a dynamic batch.
    const auto base_ndim = static_cast<int64_t>(shapes[i].size());
    TORCH_CHECK(tensor.dim() >= base_ndim &&
                    tensor.sizes().slice(tensor.dim() - base_ndim).equals(shapes[i]),
                "NEML2 input '",
                names[i],
                "' has shape ",
                tensor.sizes(),
                "; expected trailing base shape ",
                at::IntArrayRef(shapes[i]));
    batch_shape = at::infer_size(batch_shape, tensor.sizes().slice(0, tensor.dim() - base_ndim));
  }

  if (batch_shape.size() <= 1)
    return inputs;

  for (const auto & [name, parameter] : _parameters)
  {
    const auto & base = libmesh_map_find(parameter_base_shapes(), name);
    if (parameter.dim() == static_cast<int64_t>(base.size()))
      continue;
    auto shape = batch_shape;
    shape.insert(shape.end(), base.begin(), base.end());
    _m.set_parameter(name, flattenNEML2Batch(parameter.expand(shape), base));
  }

  auto flattened = inputs;
  for (const auto i : index_range(names))
  {
    auto shape = batch_shape;
    shape.insert(shape.end(), shapes[i].begin(), shapes[i].end());
    auto & tensor = libmesh_map_find(flattened, names[i]);
    // Expand first: independently flattening [nelem, 1] and [1, nqp] would
    // destroy their broadcast relationship. This also batches constant state seeds.
    tensor = flattenNEML2Batch(tensor.expand(shape), shapes[i]);
  }
  return flattened;
}

void
AOTIModelHandle::restoreOutputs(std::map<std::string, at::Tensor> & outputs,
                                const std::vector<int64_t> & batch_shape) const
{
  const auto & names = output_names();
  const auto & shapes = output_base_shapes();
  for (const auto i : index_range(names))
    restoreNEML2Batch(libmesh_map_find(outputs, names[i]), batch_shape, shapes[i].size());
}

void
AOTIModelHandle::restoreJacobian(neml2::aoti::VariablePairJacobian & jacobian,
                                 const std::vector<int64_t> & batch_shape,
                                 bool parameters) const
{
  const auto & names = output_names();
  const auto & shapes = output_base_shapes();
  auto column_shapes = parameter_base_shapes();
  if (!parameters)
  {
    column_shapes.clear();
    for (const auto i : index_range(input_names()))
      column_shapes[input_names()[i]] = input_base_shapes()[i];
  }
  for (const auto i : index_range(names))
  {
    const auto it = jacobian.find(names[i]);
    if (it == jacobian.end())
      continue;
    for (auto & [name, tensor] : it->second)
    {
      const auto base_ndim = libmesh_map_find(column_shapes, name).size();
      restoreNEML2Batch(tensor, batch_shape, shapes[i].size() + base_ndim);
    }
  }
}

std::map<std::string, at::Tensor>
AOTIModelHandle::value(const std::map<std::string, at::Tensor> & inputs) const
{
  std::vector<int64_t> batch_shape;
  auto out = _m.forward(flattenInputs(inputs, batch_shape));
  restoreOutputs(out, batch_shape);
  return out;
}

std::pair<std::map<std::string, at::Tensor>, neml2::aoti::VariablePairJacobian>
AOTIModelHandle::jacobian(const std::map<std::string, at::Tensor> & inputs) const
{
  std::vector<int64_t> batch_shape;
  auto [out, jac] = _m.jacobian(flattenInputs(inputs, batch_shape));
  restoreOutputs(out, batch_shape);
  restoreJacobian(jac, batch_shape, false);
  return {std::move(out), std::move(jac)};
}

std::pair<std::map<std::string, at::Tensor>, neml2::aoti::VariablePairJacobian>
AOTIModelHandle::param_jacobian(const std::map<std::string, at::Tensor> & inputs) const
{
  std::vector<int64_t> batch_shape;
  auto [out, jac] = _m.param_jacobian(flattenInputs(inputs, batch_shape));
  restoreOutputs(out, batch_shape);
  restoreJacobian(jac, batch_shape, true);
  return {std::move(out), std::move(jac)};
}

void
AOTIModelHandle::set_parameter(const std::string & name, const at::Tensor & value)
{
  _m.set_parameter(name, flattenNEML2Batch(value, libmesh_map_find(parameter_base_shapes(), name)));
  _parameters[name] = value;
}
#endif
