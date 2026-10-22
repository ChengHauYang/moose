//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NEML2ToKokkosMaterialProperty.h"
#include "NEML2BatchIndexGenerator.h"
#include "NEML2ModelExecutor.h"
#include "NEML2Utils.h"
#include "KokkosNEML2Device.h"

#include <algorithm>

#if defined(NEML2_ENABLED) && defined(MOOSE_ENABLE_KOKKOS_GPU) && defined(KOKKOS_ENABLE_SYCL)
// c10::xpu device queries, used to match the Kokkos SYCL device to a torch XPU device index.
#include <c10/xpu/XPUFunctions.h>
#endif

#ifdef NEML2_ENABLED
// Declared in KokkosNEML2Device.h and shared with the Kokkos->NEML2 gatherers.
at::Device
kokkosTorchDevice()
{
#ifndef MOOSE_ENABLE_KOKKOS_GPU
  return at::Device(at::kCPU);
#elif defined(KOKKOS_ENABLE_CUDA)
  return at::Device(at::kCUDA, ExecSpace().cuda_device());
#elif defined(KOKKOS_ENABLE_HIP)
  // ROCm builds of torch expose HIP devices under the CUDA device type, and both torch and Kokkos
  // index them by the HIP runtime's device ordinal.
  return at::Device(at::kCUDA, ExecSpace().hip_device());
#elif defined(KOKKOS_ENABLE_SYCL)
  // A USM pointer is only valid within the SYCL context it was allocated in, so an unmanaged View
  // over a torch allocation is addressable from the Kokkos queue only if the two share a context.
  const auto & queue = ExecSpace().sycl_queue();
  if (queue.get_context() != c10::xpu::get_device_context())
    ::mooseError("Kokkos and torch are using different SYCL contexts, so NEML2 output tensors are "
                 "not addressable from the Kokkos execution space.");

  // torch indexes its XPU devices independently of Kokkos, so map through the SYCL device itself
  // rather than assuming the two enumerations agree.
  const auto & kokkos_device = queue.get_device();
  for (c10::DeviceIndex i = 0; i < c10::xpu::device_count(); ++i)
    if (c10::xpu::get_raw_device(i) == kokkos_device)
      return at::Device(at::kXPU, i);

  ::mooseError("The SYCL device Kokkos is running on is not among the ",
               static_cast<int>(c10::xpu::device_count()),
               " XPU devices visible to torch.");
#else
  ::mooseError("NEML2ToKokkosMaterialProperty supports GPU execution only with the CUDA, HIP, "
               "and SYCL Kokkos backends.");
#endif
}
#endif

registerKokkosMaterial("MooseApp", NEML2ToKokkosRankTwoMaterialProperty);
registerKokkosMaterial("MooseApp", NEML2ToKokkosRankFourMaterialProperty);
registerKokkosMaterial("MooseApp", NEML2ToKokkosFullRankTwoMaterialProperty);
registerKokkosMaterial("MooseApp", NEML2ToKokkosFullRankFourMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosSideRankTwoMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosSideRankFourMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosSideFullRankTwoMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosSideFullRankFourMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosNeighborRankTwoMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosNeighborRankFourMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosNeighborFullRankTwoMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosNeighborFullRankFourMaterialProperty);

template <unsigned int rank, bool full, bool interface, bool neighbor>
InputParameters
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::validParams()
{
  auto params = MaterialBase::validParams();
  params += NEML2OutputInterface::validParams();
  if constexpr (neighbor)
    params.template set<Moose::MaterialDataType>("_material_data_type") =
        Moose::NEIGHBOR_MATERIAL_DATA;
  else if constexpr (interface)
    params.template set<Moose::MaterialDataType>("_material_data_type") =
        Moose::INTERFACE_MATERIAL_DATA;
  params.addClassDescription("Provides a " + std::string(full ? "full" : "symmetric") + " rank-" +
                             std::to_string(rank) +
                             " NEML2 output or derivative as a Kokkos material property.");
  params.template addRequiredParam<MaterialPropertyName>(
      "to_moose", "Kokkos material property used to store the NEML2 output or derivative.");
  return params;
}

template <unsigned int rank, bool full, bool interface, bool neighbor>
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::NEML2ToKokkosMaterialProperty(
    const InputParameters & params)
  : MaterialBase(params), NEML2OutputInterface(params, *this)
{
  _prop = this->template declareKokkosPropertyByName<Real, rank>(
      this->template getParam<MaterialPropertyName>("to_moose"),
      std::vector<unsigned int>(rank, this->_mesh.dimension()));
  NEML2Utils::assertNEML2Enabled();
#ifdef NEML2_ENABLED
  requestNEML2OutputDevice(kokkosTorchDevice());
#endif
}

template <unsigned int rank, bool full, bool interface, bool neighbor>
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::NEML2ToKokkosMaterialProperty(
    const NEML2ToKokkosMaterialProperty & object)
  : MaterialBase(object),
    NEML2OutputInterface(object),
    _prop(object._prop),
    _staged_output(object._staged_output),
    _batch_offset(object._batch_offset),
    _batch_mapping_outdated(object._batch_mapping_outdated),
    _broadcast_output(object._broadcast_output)
{
}

template <unsigned int rank, bool full, bool interface, bool neighbor>
void
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::meshChanged()
{
  _batch_mapping_outdated = true;
}

template <unsigned int rank, bool full, bool interface, bool neighbor>
void
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::computeProperties()
{
#ifdef NEML2_ENABLED
  constexpr unsigned int num_components = full ? (rank == 2 ? 9 : 81) : (rank == 2 ? 6 : 36);
  if (!outputReady())
  {
    if (_staged_output.size() != num_components || !_broadcast_output)
    {
      this->_compute_dispatcher.reset();
      _staged_output.create(num_components);
      _broadcast_output = true;
    }
#ifdef MOOSE_ENABLE_KOKKOS_GPU
    ::Kokkos::deep_copy(_staged_output.deviceView(), 0.0);
#else
    std::fill_n(_staged_output.hostData(), num_components, 0);
#endif
    const auto map_size = [&]()
    {
      if constexpr (interface)
        return this->kokkosInterfaceFaces().size();
      else
        return this->kokkosMesh().getNumLocalElements();
    }();
    if (_batch_offset.size() != map_size)
    {
      this->_compute_dispatcher.reset();
      _batch_offset.create(map_size);
    }
    std::fill_n(_batch_offset.hostData(), map_size, libMesh::DofObject::invalid_id);
    _batch_offset.copyToDevice();
    _batch_mapping_outdated = false;
    _staged_output.copyToDevice();
    MaterialBase::computeProperties();
    return;
  }

  if (!neml2Output().defined())
  {
    if (_staged_output.size() != num_components || !_broadcast_output)
    {
      this->_compute_dispatcher.reset();
      _staged_output.create(num_components);
      _broadcast_output = true;
    }
#ifdef MOOSE_ENABLE_KOKKOS_GPU
    ::Kokkos::deep_copy(_staged_output.deviceView(), 0.0);
#else
    std::fill_n(_staged_output.hostData(), num_components, 0);
#endif

    const auto map_size = [&]()
    {
      if constexpr (interface)
        return this->kokkosInterfaceFaces().size();
      else
        return this->kokkosMesh().getNumLocalElements();
    }();
    if (_batch_offset.size() != map_size)
    {
      this->_compute_dispatcher.reset();
      _batch_offset.create(map_size);
    }
    std::fill_n(_batch_offset.hostData(), map_size, libMesh::DofObject::invalid_id);
    _batch_offset.copyToDevice();
    _batch_mapping_outdated = false;
  }
  else
  {
    const auto base_ndim = static_cast<int64_t>(full ? rank : rank / 2);
    const auto batch_ndim = neml2Output().dim() - base_ndim;
    const auto expected_shape = full        ? std::vector<int64_t>(rank, 3)
                                : rank == 2 ? std::vector<int64_t>{6}
                                            : std::vector<int64_t>{6, 6};
    const auto actual_shape = neml2Output().sizes().slice(batch_ndim);
    if (batch_ndim < 0 || batch_ndim > 2 || !actual_shape.equals(expected_shape))
      mooseError("Cannot copy NEML2 tensor with shape ",
                 neml2Output().sizes(),
                 " into a ",
                 full ? "full" : "symmetric",
                 " rank-",
                 rank,
                 " Kokkos material property; expected base shape ",
                 expected_shape,
                 ".");
    if (neml2Output().dtype() != at::kDouble)
      mooseError("Cannot copy NEML2 tensor with dtype ",
                 neml2Output().dtype(),
                 " into a Real Kokkos material property.");

    const auto kokkos_device = kokkosTorchDevice();
    if (neml2Output().device() != kokkos_device)
      mooseError("Cannot copy NEML2 output on ",
                 neml2Output().device(),
                 " into a Kokkos material property on ",
                 kokkos_device,
                 ". Set the NEML2 executor output_device to ",
                 kokkos_device,
                 ".");

    const auto output = neml2Output().contiguous();
    const auto batch_size = static_cast<std::size_t>(output.numel() / num_components);
    const bool broadcast_output = batch_ndim == 0;
    if (_staged_output.size() != static_cast<std::size_t>(output.numel()) ||
        _broadcast_output != broadcast_output)
    {
      this->_compute_dispatcher.reset();
      _staged_output.create(output.numel());
      _broadcast_output = broadcast_output;
    }
#ifdef MOOSE_ENABLE_KOKKOS_GPU
    const auto output_view =
        ::Kokkos::View<const Real *, MemSpace, ::Kokkos::MemoryTraits<::Kokkos::Unmanaged>>(
            output.template data_ptr<Real>(), output.numel());
    ::Kokkos::deep_copy(_staged_output.deviceView(), output_view);
#else
    std::copy_n(output.template data_ptr<Real>(), output.numel(), _staged_output.hostData());
#endif

    const auto map_size = [&]()
    {
      if constexpr (interface)
        return this->kokkosInterfaceFaces().size();
      else
        return this->kokkosMesh().getNumLocalElements();
    }();
    if (_batch_mapping_outdated || _batch_offset.size() != map_size)
    {
      if (_batch_offset.size() != map_size)
      {
        this->_compute_dispatcher.reset();
        _batch_offset.create(map_size);
      }

      if constexpr (interface)
      {
        // An interface restriction can leave a cached face without a corresponding NEML2 side.
        // Initialize every entry so an erroneous mapping can never expose uninitialized device
        // data.
        std::fill_n(_batch_offset.hostData(), map_size, libMesh::DofObject::invalid_id);
        for (const auto face_index : index_range(this->kokkosInterfaceFaces()))
        {
          const auto & face = this->kokkosInterfaceFaces()[face_index];
          const auto elem_side =
              NEML2BatchIndexGenerator::ElemSide(neighbor ? face.neighbor_id : face.elem_id,
                                                 neighbor ? face.neighbor_side : face.side);
          if (_execute_neml2_model.isSideBatchIndexExist(elem_side))
          {
            // The staged tensor is flat even when NEML2 retains a two-dimensional batch shape.
            // Its side start therefore always comes from the executor's stable ElemSide map, not
            // from the Kokkos-oriented face index.
            const auto batch_offset = _execute_neml2_model.getSideBatchIndex(elem_side);
            _batch_offset.hostData()[face.face_index] = batch_offset;
            if (!_broadcast_output && batch_offset + face.n_qps > batch_size)
              mooseError("Interface element side (",
                         std::get<0>(elem_side),
                         ", ",
                         std::get<1>(elem_side),
                         ") maps outside NEML2 output tensor with shape ",
                         output.sizes(),
                         ".");
          }
        }
      }
      else
      {
        for (const auto elem : this->_mesh.getMesh().active_local_element_ptr_range())
        {
          const auto elem_id = this->kokkosMesh().getContiguousElementID(elem);
          _batch_offset.hostData()[elem_id] = neml2BatchIndex(elem->id());
          const auto num_qps = this->kokkosAssembly().getNumQpsHost(elem_id);
          if (!_broadcast_output && _batch_offset.hostData()[elem_id] + num_qps > batch_size)
            mooseError("Element ",
                       elem->id(),
                       " maps outside NEML2 output tensor with shape ",
                       output.sizes(),
                       ".");
        }
      }

      _batch_offset.copyToDevice();
      _batch_mapping_outdated = false;
    }
  }

#ifndef MOOSE_ENABLE_KOKKOS_GPU
  _staged_output.copyToDevice();
#endif
  MaterialBase::computeProperties();
#else
  NEML2Utils::assertNEML2Enabled();
#endif
}

template class NEML2ToKokkosMaterialProperty<2>;
template class NEML2ToKokkosMaterialProperty<4>;
template class NEML2ToKokkosMaterialProperty<2, true>;
template class NEML2ToKokkosMaterialProperty<4, true>;
template class NEML2ToKokkosMaterialProperty<2, false, true>;
template class NEML2ToKokkosMaterialProperty<4, false, true>;
template class NEML2ToKokkosMaterialProperty<2, false, true, true>;
template class NEML2ToKokkosMaterialProperty<4, false, true, true>;
