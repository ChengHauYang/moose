//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosQuantityToNEML2.h"

#ifdef MOOSE_KOKKOS_ENABLED

#include "KokkosDispatcher.h"
#ifdef NEML2_ENABLED
#include "KokkosNEML2Device.h"
registerKokkosUserObject("MooseApp", KokkosQuantityToNEML2);
#endif

InputParameters
KokkosQuantityToNEML2::validParams()
{
  InputParameters params = Moose::Kokkos::ElementUserObject::validParams();
  params += MOOSEToNEML2::validParams();
  params.addClassDescription(
      "Gather a current-state rank-two Kokkos material property into a device buffer for direct "
      "(zero-copy on CUDA) insertion into a NEML2 input tensor. Full tensors use row-major "
      "components; symmetric tensors use six-component Mandel packing. "
      "NEML2BatchIndexGenerator (execution_order_group = -1) supplies batch row destinations; "
      "this gatherer runs in group 0.");
  params.addRequiredParam<MaterialPropertyName>(
      "from_moose", "Name of the rank-two Kokkos material property to gather");
  params.addParam<bool>(
      "symmetric", false, "Gather the property using six-component Mandel packing.");
  params.addRequiredParam<UserObjectName>(
      "batch_index_generator", "The NEML2BatchIndexGenerator that supplies bulk batch offsets");
  ExecFlagEnum execute_options = MooseUtils::getDefaultExecFlagEnum();
  execute_options = {EXEC_INITIAL, EXEC_LINEAR, EXEC_NONLINEAR};
  params.set<ExecFlagEnum>("execute_on") = execute_options;
  return params;
}

KokkosQuantityToNEML2::KokkosQuantityToNEML2(const InputParameters & parameters)
  : Moose::Kokkos::ElementUserObject(parameters),
    MOOSEToNEML2(parameters),
    _batch_index_generator(getUserObject<NEML2BatchIndexGenerator>("batch_index_generator")),
    _mat_prop(getKokkosMaterialProperty<Real, 2>(getParam<MaterialPropertyName>("from_moose"))),
    _symmetric(getParam<bool>("symmetric")),
    _num_components(_symmetric ? 6 : 9)
{
}

KokkosQuantityToNEML2::KokkosQuantityToNEML2(const KokkosQuantityToNEML2 & object)
  : Moose::Kokkos::ElementUserObject(object),
    MOOSEToNEML2(object),
    _batch_index_generator(object._batch_index_generator),
    _mat_prop(object._mat_prop),
    _symmetric(object._symmetric),
    _num_components(object._num_components),
    _elem_batch_indices(object._elem_batch_indices),
    _buffer(object._buffer),
    _last_map_generation(object._last_map_generation)
#ifdef NEML2_ENABLED
    ,
    _gathered_tensor(object._gathered_tensor)
#endif
{
}

void
KokkosQuantityToNEML2::initialize()
{
  // Refresh the device-accessible element batch index array from the generator whenever the
  // generator rebuilds its mirrors (mesh change or first execute).
  const auto gen = _batch_index_generator.mapGeneration();
  if (gen != _last_map_generation)
  {
    const auto total_batch = _batch_index_generator.getBatchIndex();
    _buffer.create(total_batch * _num_components);
    _elem_batch_indices = _batch_index_generator.kokkosElementBatchIndices();
    _last_map_generation = gen;
    _user_object_dispatcher.reset();
  }

  _buffer = 0.0;
}

void
KokkosQuantityToNEML2::compute()
{
  Moose::Kokkos::ElementUserObject::compute();
}

void
KokkosQuantityToNEML2::finalize()
{
#ifdef NEML2_ENABLED
  // Fence so all Kokkos kernel writes are visible before Torch observes the buffer.
  ::Kokkos::fence();

  const auto total_batch = _batch_index_generator.getBatchIndex();
  // deviceData() lives in the Kokkos execution space's memory (host for a CPU build, the GPU for a
  // CUDA build), so the wrapping tensor must be labeled with that device; NEML2ModelExecutor then
  // transfers it to the requested NEML2 device. See KokkosNEML2Device.h.
  const at::Device torch_device(kokkosTorchDevice());
  const auto shape = _symmetric ? std::vector<int64_t>{static_cast<int64_t>(total_batch), 6}
                                : std::vector<int64_t>{static_cast<int64_t>(total_batch), 3, 3};
  _gathered_tensor = at::from_blob(
      _buffer.deviceData(), shape, at::TensorOptions().dtype(at::kDouble).device(torch_device));
#endif
}

#ifdef NEML2_ENABLED
at::Tensor
KokkosQuantityToNEML2::gatheredData() const
{
  return _gathered_tensor;
}
#endif

#endif // MOOSE_KOKKOS_ENABLED
