//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NEML2ToKokkosInterfaceMaterialProperty.h"
#include "NEML2BatchIndexGenerator.h"
#include "NEML2ModelExecutor.h"
#include "NEML2Utils.h"

#include <algorithm>

#if defined(NEML2_ENABLED) && defined(MOOSE_ENABLE_KOKKOS_GPU) && defined(KOKKOS_ENABLE_SYCL)
#include <c10/xpu/XPUFunctions.h>
#endif

#ifdef NEML2_ENABLED
namespace
{
at::Device
interfaceKokkosTorchDevice()
{
#ifndef MOOSE_ENABLE_KOKKOS_GPU
  return at::Device(at::kCPU);
#elif defined(KOKKOS_ENABLE_CUDA)
  return at::Device(at::kCUDA, Kokkos::DefaultExecutionSpace().cuda_device());
#elif defined(KOKKOS_ENABLE_HIP)
  return at::Device(at::kHIP, Kokkos::DefaultExecutionSpace().hip_device());
#elif defined(KOKKOS_ENABLE_SYCL)
  return at::Device(at::kXPU, c10::xpu::current_device());
#else
  return at::Device(at::kCPU);
#endif
}
}
#endif

registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosSideRankTwoMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosSideRankFourMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosNeighborRankTwoMaterialProperty);
registerKokkosInterfaceMaterial("MooseApp", NEML2ToKokkosNeighborRankFourMaterialProperty);

template <unsigned int rank, bool neighbor>
InputParameters
NEML2ToKokkosInterfaceMaterialProperty<rank, neighbor>::validParams()
{
  auto params = Moose::Kokkos::InterfaceMaterial::validParams();
  params += NEML2OutputInterface::validParams();
  params.set<Moose::MaterialDataType>("_material_data_type") =
      neighbor ? Moose::NEIGHBOR_MATERIAL_DATA : Moose::INTERFACE_MATERIAL_DATA;
  params.addClassDescription("Provides a symmetric rank-" + std::to_string(rank) +
                             " NEML2 output or derivative as a Kokkos interface material property.");
  params.addRequiredParam<MaterialPropertyName>(
      "to_moose", "Kokkos material property used to store the NEML2 output or derivative.");
  return params;
}

template <unsigned int rank, bool neighbor>
NEML2ToKokkosInterfaceMaterialProperty<rank, neighbor>::NEML2ToKokkosInterfaceMaterialProperty(
    const InputParameters & params)
  : InterfaceMaterial(params),
    NEML2OutputInterface(params, *this),
    _prop(declareKokkosProperty<Real, rank>("to_moose",
                                            std::vector<unsigned int>(rank, _mesh.dimension())))
{
  NEML2Utils::assertNEML2Enabled();
#ifdef NEML2_ENABLED
  requestNEML2OutputDevice(interfaceKokkosTorchDevice());
#endif
}

template <unsigned int rank, bool neighbor>
NEML2ToKokkosInterfaceMaterialProperty<rank, neighbor>::NEML2ToKokkosInterfaceMaterialProperty(
    const NEML2ToKokkosInterfaceMaterialProperty & object)
  : InterfaceMaterial(object),
    NEML2OutputInterface(object),
    _prop(object._prop),
    _staged_output(object._staged_output),
    _side_batch_offset(object._side_batch_offset),
    _batch_mapping_outdated(object._batch_mapping_outdated),
    _broadcast_output(object._broadcast_output),
    _batch_qp_stride(object._batch_qp_stride)
{
}

template <unsigned int rank, bool neighbor>
void
NEML2ToKokkosInterfaceMaterialProperty<rank, neighbor>::meshChanged()
{
  _batch_mapping_outdated = true;
}

template <unsigned int rank, bool neighbor>
void
NEML2ToKokkosInterfaceMaterialProperty<rank, neighbor>::computeProperties()
{
#ifdef NEML2_ENABLED
  if (!outputReady())
    return;

  constexpr unsigned int num_components = rank == 2 ? 6 : 36;
  if (!neml2Output().defined())
  {
    if (_staged_output.size() != num_components || !_broadcast_output)
    {
      _compute_dispatcher.reset();
      _staged_output.create(num_components);
      _broadcast_output = true;
    }
#ifdef MOOSE_ENABLE_KOKKOS_GPU
    ::Kokkos::deep_copy(_staged_output.deviceView(), 0.0);
#else
    std::fill_n(_staged_output.hostData(), num_components, 0);
#endif
  }
  else
  {
    const auto base_ndim = static_cast<int64_t>(rank / 2);
    const auto batch_ndim = neml2Output().dim() - base_ndim;
    const auto expected_shape = rank == 2 ? std::vector<int64_t>{6} : std::vector<int64_t>{6, 6};
    const auto actual_shape = neml2Output().sizes().slice(batch_ndim);
    if (batch_ndim < 0 || batch_ndim > 2 || !actual_shape.equals(expected_shape))
      mooseError("Cannot copy NEML2 tensor with shape ",
                 neml2Output().sizes(),
                 " into a symmetric rank-",
                 rank,
                 " Kokkos interface material property.");
    if (neml2Output().dtype() != at::kDouble)
      mooseError("Cannot copy a non-double NEML2 output into a Real Kokkos material property.");
    if (neml2Output().device() != interfaceKokkosTorchDevice())
      mooseError("NEML2 output device does not match the Kokkos execution device.");

    const auto output = neml2Output().contiguous();
    const auto batch_size = static_cast<std::size_t>(output.numel() / num_components);
    const bool broadcast_output = batch_ndim == 0;
    if (_staged_output.size() != static_cast<std::size_t>(output.numel()) ||
        _broadcast_output != broadcast_output)
    {
      _compute_dispatcher.reset();
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

    const auto batch_qp_stride = batch_ndim == 2 ? output.size(1) : 0;
    const auto & faces = kokkosInterfaceFaces();
    const auto num_face_qps =
        faces.size() ? faces[faces.size() - 1].qp_map_offset + faces[faces.size() - 1].n_qps : 0;
    if (_batch_mapping_outdated || _side_batch_offset.size() != num_face_qps ||
        _batch_qp_stride != batch_qp_stride)
    {
      if (_side_batch_offset.size() != num_face_qps)
      {
        _compute_dispatcher.reset();
        _side_batch_offset.create(num_face_qps);
      }

      for (const auto face_index : index_range(faces))
      {
        const auto & face = faces[face_index];
        const auto elem_side = NEML2BatchIndexGenerator::ElemSide(
            neighbor ? face.neighbor_id : face.elem_id, neighbor ? face.neighbor_side : face.side);
        if (!_execute_neml2_model.isSideBatchIndexExist(elem_side))
          mooseError("No NEML2 batch index exists for interface element side (",
                     std::get<0>(elem_side),
                     ", ",
                     std::get<1>(elem_side),
                     ").");
        const auto batch_offset = batch_ndim == 1 ? _execute_neml2_model.getSideBatchIndex(elem_side)
                                                   : face_index * batch_qp_stride;
        for (const auto qp : make_range(face.n_qps))
          _side_batch_offset.hostData()[face.qp_map_offset + qp] = batch_offset;
        if (!_broadcast_output && batch_offset + face.n_qps > batch_size)
          mooseError("Interface face maps outside NEML2 output tensor with shape ", output.sizes(), ".");
      }
      _side_batch_offset.copyToDevice();
      _batch_mapping_outdated = false;
      _batch_qp_stride = batch_qp_stride;
    }
  }

#ifndef MOOSE_ENABLE_KOKKOS_GPU
  _staged_output.copyToDevice();
#endif
  InterfaceMaterial::computeProperties();
#else
  NEML2Utils::assertNEML2Enabled();
#endif
}

template class NEML2ToKokkosInterfaceMaterialProperty<2, false>;
template class NEML2ToKokkosInterfaceMaterialProperty<4, false>;
template class NEML2ToKokkosInterfaceMaterialProperty<2, true>;
template class NEML2ToKokkosInterfaceMaterialProperty<4, true>;
