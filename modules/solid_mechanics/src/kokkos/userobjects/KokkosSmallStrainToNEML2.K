//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosSmallStrainToNEML2.h"
#include "NEML2Utils.h"

registerKokkosUserObject("SolidMechanicsApp", KokkosSmallStrainToNEML2);

InputParameters
KokkosSmallStrainToNEML2::validParams()
{
  auto params = MOOSEToNEML2::validParams();
  params += Moose::Kokkos::ElementUserObject::validParams();
  params.addClassDescription(
      "Computes infinitesimal strain from Kokkos displacement gradients and supplies it to NEML2.");
  params.addRequiredCoupledVar("displacements", "Displacement variables in Cartesian order.");
  params.addParam<bool>("moose_to_neml2_on_gpu",
                        false,
                        "Supply the device-resident strain buffer directly to NEML2 instead of "
                        "staging it through host memory.");
  params.set<ExecFlagEnum>("execute_on") = {EXEC_INITIAL, EXEC_LINEAR, EXEC_NONLINEAR};
  return params;
}

KokkosSmallStrainToNEML2::KokkosSmallStrainToNEML2(const InputParameters & parameters)
  : Moose::Kokkos::ElementUserObject(parameters),
    MOOSEToNEML2(parameters),
    _ndisp(coupledComponents("displacements")),
    _moose_to_neml2_on_gpu(getParam<bool>("moose_to_neml2_on_gpu")),
    _grad_displacements(kokkosCoupledGradients("displacements"))
{
  NEML2Utils::assertNEML2Enabled();

  // Declare the current-solution vector-tag dependency so computeKokkosUserObjectsInternal syncs
  // the current Newton iterate to device and reinits QP gradients before this UO's parallel
  // dispatch reads them. Without this, execute() reads stale/zero QP gradients and delivers zero
  // strain to NEML2 (predictor trivially converges, plasticity never triggers).
  addFEVariableCoupleableVectorTag(_fe_problem.getVectorTagID(Moose::SOLUTION_TAG));

  if (_ndisp != _fe_problem.mesh().dimension())
    paramError("displacements",
               "The number of displacement variables must match the mesh dimension.");
}

KokkosSmallStrainToNEML2::KokkosSmallStrainToNEML2(const KokkosSmallStrainToNEML2 & object)
  : Moose::Kokkos::ElementUserObject(object),
    MOOSEToNEML2(object),
    _ndisp(object._ndisp),
    _moose_to_neml2_on_gpu(object._moose_to_neml2_on_gpu),
    _grad_displacements(object._grad_displacements),
    _buffer(object._buffer),
    _element_batch_offset(object._element_batch_offset)
{
}

void
KokkosSmallStrainToNEML2::initialize()
{
  const auto num_elems = kokkosMesh().getNumLocalElements();
  bool reallocated = false;

  if (_element_batch_offset.size() != num_elems)
  {
    _element_batch_offset.create(num_elems);
    reallocated = true;
  }

  dof_id_type num_qps = 0;
  for (const auto local_elem_index : make_range(num_elems))
  {
    _element_batch_offset.hostData()[local_elem_index] = num_qps;
    num_qps += kokkosAssembly().getNumQpsHost(local_elem_index);
  }
  _element_batch_offset.copyToDevice();

  if (_buffer.size() != 6 * num_qps)
  {
    _buffer.create(6 * num_qps);
    reallocated = true;
  }

  if (reallocated)
    _user_object_dispatcher.reset();
}

#ifdef NEML2_ENABLED
at::Tensor
KokkosSmallStrainToNEML2::gatheredData() const
{
  mooseAssert(_buffer.isAlloc(),
              "The strain buffer must be created before gatheredData is called.");

  const auto num_elems = static_cast<int64_t>(_element_batch_offset.size());
  mooseAssert(num_elems > 0,
              "The element batch offsets must be built before gatheredData is called.");
  const auto num_qps = static_cast<int64_t>(_buffer.size() / (6 * num_elems));
  mooseAssert(num_qps > 0, "An element must have at least one quadrature point.");
  const std::vector<int64_t> shape = {num_elems, num_qps, 6};

#ifdef MOOSE_ENABLE_KOKKOS_GPU
#ifdef KOKKOS_ENABLE_CUDA
  if (_moose_to_neml2_on_gpu)
  {
    // Kokkos and libtorch may use different CUDA streams. Complete the strain kernel before
    // exposing its allocation to libtorch.
    ::Kokkos::fence();
    const auto options =
        at::TensorOptions().dtype(at::kDouble).device(at::kCUDA, ExecSpace().cuda_device());
    return at::from_blob(_buffer.deviceData(), shape, options);
  }

  _buffer.copyToHost();
  return at::from_blob(_buffer.hostData(), shape, at::TensorOptions().dtype(at::kDouble))
      .to(at::Device(at::kCUDA, ExecSpace().cuda_device()));
#else
  mooseError("KokkosSmallStrainToNEML2 supports GPU execution only with CUDA.");
#endif
#else
  return at::from_blob(_buffer.deviceData(), shape, at::TensorOptions().dtype(at::kDouble));
#endif
}
#endif
