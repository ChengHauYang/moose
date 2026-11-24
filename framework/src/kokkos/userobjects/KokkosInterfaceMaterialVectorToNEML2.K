//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosInterfaceMaterialVectorToNEML2.h"

#ifdef MOOSE_KOKKOS_ENABLED

#include "KokkosDispatcher.h"
#ifdef NEML2_ENABLED
#include "KokkosNEML2Device.h"
registerKokkosInterfaceUserObject("MooseApp", KokkosInterfaceMaterialVectorToNEML2);
#endif

InputParameters
KokkosInterfaceMaterialVectorToNEML2::validParams()
{
  InputParameters params = Moose::Kokkos::InterfaceUserObject::validParams();
  params += MOOSEToNEML2::validParams();
  params.addClassDescription(
      "Gather a current-state vector Kokkos interface material property, with one value per "
      "interface quadrature point (e.g. a cohesive displacement jump), into a device buffer for "
      "direct (zero-copy on CUDA) insertion into a NEML2 input tensor. An interface-only "
      "NEML2BatchIndexGenerator (volume = false) in an earlier execution_order_group supplies "
      "batch row destinations.");
  params.addRequiredParam<MaterialPropertyName>(
      "from_moose", "Name of the vector Kokkos interface material property");
  params.addRequiredParam<UserObjectName>(
      "batch_index_generator",
      "The interface-only NEML2BatchIndexGenerator that supplies face batch offsets");
  ExecFlagEnum execute_options = MooseUtils::getDefaultExecFlagEnum();
  execute_options = {EXEC_INITIAL, EXEC_LINEAR, EXEC_NONLINEAR};
  params.set<ExecFlagEnum>("execute_on") = execute_options;
  return params;
}

KokkosInterfaceMaterialVectorToNEML2::KokkosInterfaceMaterialVectorToNEML2(
    const InputParameters & parameters)
  : Moose::Kokkos::InterfaceUserObject(parameters),
    MOOSEToNEML2(parameters),
    _batch_index_generator(getUserObject<NEML2BatchIndexGenerator>("batch_index_generator")),
    _mat_prop(getKokkosMaterialProperty<Real, 1>(getParam<MaterialPropertyName>("from_moose")))
{
  if (_batch_index_generator.getParam<int>("execution_order_group") >=
      getParam<int>("execution_order_group"))
    paramError("batch_index_generator",
               "The batch index generator must execute in an earlier execution_order_group than "
               "this gatherer because Kokkos user objects run before host user objects within a "
               "group.");
}

KokkosInterfaceMaterialVectorToNEML2::KokkosInterfaceMaterialVectorToNEML2(
    const KokkosInterfaceMaterialVectorToNEML2 & object)
  : Moose::Kokkos::InterfaceUserObject(object),
    MOOSEToNEML2(object),
    _batch_index_generator(object._batch_index_generator),
    _mat_prop(object._mat_prop),
    _face_primary_batch_indices(object._face_primary_batch_indices),
    _face_neighbor_batch_indices(object._face_neighbor_batch_indices),
    _buffer(object._buffer),
    _last_map_generation(object._last_map_generation)
#ifdef NEML2_ENABLED
    ,
    _gathered_tensor(object._gathered_tensor)
#endif
{
}

void
KokkosInterfaceMaterialVectorToNEML2::initialize()
{
  // Reallocate the flat buffer when the generator rebuilds its mirrors (mesh change or first
  // execute after the generator has run). Generation 0 means the generator has not yet published
  // its mirrors; defer allocation until it has.
  const auto gen = _batch_index_generator.mapGeneration();
  if (gen != 0 && gen != _last_map_generation)
  {
    _buffer.create(_batch_index_generator.getBatchIndex() * 3);
    _last_map_generation = gen;
  }

  if (_buffer.size() > 0)
    _buffer = 0.0;
}

void
KokkosInterfaceMaterialVectorToNEML2::compute()
{
  // Skip until the generator has published its Kokkos mirrors (mapGeneration > 0).
  if (_batch_index_generator.mapGeneration() == 0)
    return;

  _face_primary_batch_indices = _batch_index_generator.kokkosFacePrimaryBatchIndices();
  _face_neighbor_batch_indices = _batch_index_generator.kokkosFaceNeighborBatchIndices();
  Moose::Kokkos::InterfaceUserObject::compute();
}

void
KokkosInterfaceMaterialVectorToNEML2::finalize()
{
#ifdef NEML2_ENABLED
  ::Kokkos::fence();

  const auto total_batch = _batch_index_generator.getBatchIndex();
  // deviceData() lives in the Kokkos execution space's memory, so the wrapping tensor is labeled
  // with that device; NEML2ModelExecutor then transfers it to the requested NEML2 device. A rank
  // that owns no interface faces contributes a device-resident empty tensor (see
  // KokkosInterfaceQuantityToNEML2::finalize()).
  const at::Device torch_device(kokkosTorchDevice());
  const auto opts = at::TensorOptions().dtype(at::kDouble).device(torch_device);
  if (total_batch == 0)
    _gathered_tensor = at::empty({0, 3}, opts);
  else
    _gathered_tensor =
        at::from_blob(_buffer.deviceData(), {static_cast<int64_t>(total_batch), 3}, opts);
#endif
}

#ifdef NEML2_ENABLED
at::Tensor
KokkosInterfaceMaterialVectorToNEML2::gatheredData() const
{
  return _gathered_tensor;
}
#endif

#endif // MOOSE_KOKKOS_ENABLED
