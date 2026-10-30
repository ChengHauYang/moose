//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosInterfaceQuantityToNEML2.h"

#ifdef MOOSE_KOKKOS_ENABLED

#include "KokkosDispatcher.h"
#ifdef NEML2_ENABLED
#include "KokkosNEML2Device.h"
registerKokkosInterfaceUserObject("MooseApp", KokkosInterfaceQuantityToNEML2);
#endif

InputParameters
KokkosInterfaceQuantityToNEML2::validParams()
{
  InputParameters params = Moose::Kokkos::InterfaceUserObject::validParams();
  params += MOOSEToNEML2::validParams();
  params.addClassDescription(
      "Gather current-state full rank-two Kokkos interface material properties from both sides of "
      "a cohesive interface into a device buffer for direct (zero-copy on CUDA) insertion into a "
      "NEML2 input tensor. NEML2BatchIndexGenerator (interface_only = true, "
      "execution_order_group = -1) supplies batch row destinations; this gatherer runs in group "
      "0.");
  params.addRequiredParam<MaterialPropertyName>(
      "primary_from_moose",
      "Name of the rank-two Kokkos interface material property on the primary side");
  params.addRequiredParam<MaterialPropertyName>(
      "neighbor_from_moose",
      "Name of the rank-two Kokkos interface material property on the neighbor side");
  params.addRequiredParam<UserObjectName>(
      "batch_index_generator",
      "The NEML2BatchIndexGenerator (interface_only = true) that supplies face batch offsets");
  ExecFlagEnum execute_options = MooseUtils::getDefaultExecFlagEnum();
  execute_options = {EXEC_INITIAL, EXEC_LINEAR, EXEC_NONLINEAR};
  params.set<ExecFlagEnum>("execute_on") = execute_options;
  return params;
}

KokkosInterfaceQuantityToNEML2::KokkosInterfaceQuantityToNEML2(const InputParameters & parameters)
  : Moose::Kokkos::InterfaceUserObject(parameters),
    MOOSEToNEML2(parameters),
    _batch_index_generator(getUserObject<NEML2BatchIndexGenerator>("batch_index_generator")),
    _primary_mat_prop(
        getKokkosMaterialProperty<Real, 2>(getParam<MaterialPropertyName>("primary_from_moose"))),
    _neighbor_mat_prop(getKokkosNeighborMaterialProperty<Real, 2>(
        getParam<MaterialPropertyName>("neighbor_from_moose")))
{
}

KokkosInterfaceQuantityToNEML2::KokkosInterfaceQuantityToNEML2(
    const KokkosInterfaceQuantityToNEML2 & object)
  : Moose::Kokkos::InterfaceUserObject(object),
    MOOSEToNEML2(object),
    _batch_index_generator(object._batch_index_generator),
    _primary_mat_prop(object._primary_mat_prop),
    _neighbor_mat_prop(object._neighbor_mat_prop),
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
KokkosInterfaceQuantityToNEML2::initialize()
{
  // Reallocate the flat buffer when the generator rebuilds its mirrors (mesh change or first
  // execute after the generator has run).  Generation 0 means the generator has not yet
  // published its mirrors; defer allocation until it has.
  const auto gen = _batch_index_generator.mapGeneration();
  if (gen != 0 && gen != _last_map_generation)
  {
    _buffer.create(_batch_index_generator.getBatchIndex() * 9);
    _last_map_generation = gen;
  }

  if (_buffer.size() > 0)
    _buffer = 0.0;
}

void
KokkosInterfaceQuantityToNEML2::compute()
{
  // Skip until the generator has published its Kokkos mirrors (mapGeneration > 0).
  if (_batch_index_generator.mapGeneration() == 0)
    return;

  _face_primary_batch_indices = _batch_index_generator.kokkosFacePrimaryBatchIndices();
  _face_neighbor_batch_indices = _batch_index_generator.kokkosFaceNeighborBatchIndices();
  Moose::Kokkos::InterfaceUserObject::compute();
}

void
KokkosInterfaceQuantityToNEML2::finalize()
{
#ifdef NEML2_ENABLED
  ::Kokkos::fence();

  const auto total_batch = _batch_index_generator.getBatchIndex();
  // deviceData() lives in the Kokkos execution space's memory, so the wrapping tensor (and the
  // empty zero-row tensor below) must be labeled with that device; NEML2ModelExecutor then
  // transfers it to the requested NEML2 device. See KokkosNEML2Device.h.
  const at::Device torch_device(kokkosTorchDevice());
  const auto opts = at::TensorOptions().dtype(at::kDouble).device(torch_device);

  // A distributed partition may own no cohesive interface faces, so this rank contributes zero
  // batch rows. _buffer is then never allocated and deviceData() is null; at::from_blob with a
  // CUDA device rejects a null/host pointer via libtorch's pointer-device-registration check.
  // Return a device-resident empty tensor instead. This is the correct zero-row result, not a
  // workaround: NEML2ModelExecutor short-circuits an empty batch (isEmpty()) so it is never read
  // for model evaluation, and the gather oracle treats size(0) == 0 as zero checked rows. No host
  // copy, mirror, or .cpu() staging is introduced.
  if (total_batch == 0)
    _gathered_tensor = at::empty({0, 3, 3}, opts);
  else
    _gathered_tensor =
        at::from_blob(_buffer.deviceData(), {static_cast<int64_t>(total_batch), 3, 3}, opts);
#endif
}

#ifdef NEML2_ENABLED
at::Tensor
KokkosInterfaceQuantityToNEML2::gatheredData() const
{
  return _gathered_tensor;
}
#endif

#endif // MOOSE_KOKKOS_ENABLED
