//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosQuantityToNEML2.h"
#include "NEML2Utils.h"
#include "MooseVariable.h"
#include "VectorMooseVariable.h"

namespace
{
template <typename T>
MooseVariableFieldBase &
getMooseVariable(FEProblemBase & problem, THREAD_ID tid, const std::string & name)
{
  if constexpr (std::is_same_v<T, Real>)
    return problem.getStandardVariable(tid, name);
  else if constexpr (std::is_same_v<T, RealVectorValue>)
    return problem.getVectorVariable(tid, name);
}

template <typename T, unsigned int state>
MooseVariableFieldBase &
prepareVariableAndState(FEProblemBase & problem, THREAD_ID tid, const std::string & name)
{
  auto & var = getMooseVariable<T>(problem, tid, name);
  if constexpr (state == 1)
    var.sys().needSolutionState(1);
  else if constexpr (state == 2)
    var.sys().needSolutionState(2);
  return var;
}

const TagName &
stateTagName(unsigned int state)
{
  if (state == 0)
    return Moose::SOLUTION_TAG;
  else if (state == 1)
    return Moose::OLD_SOLUTION_TAG;
  else if (state == 2)
    return Moose::OLDER_SOLUTION_TAG;
  mooseError("KokkosQuantityToNEML2 only supports state <= 2 (current, old, older).");
}
}

registerKokkosUserObject("MooseApp", KokkosRealToNEML2);
registerKokkosUserObject("MooseApp", KokkosOldRealToNEML2);
registerKokkosUserObject("MooseApp", KokkosOlderRealToNEML2);

registerKokkosUserObject("MooseApp", KokkosRealVectorValueToNEML2);
registerKokkosUserObject("MooseApp", KokkosOldRealVectorValueToNEML2);
registerKokkosUserObject("MooseApp", KokkosOlderRealVectorValueToNEML2);

template <typename T, unsigned int state>
InputParameters
KokkosQuantityToNEML2<T, state>::validParams()
{
  auto params = MOOSEToNEML2::validParams();
  params += Moose::Kokkos::ElementUserObject::validParams();

  params.addClassDescription(
      "Gather a MOOSE variable into a persistent device buffer for insertion into the NEML2 model "
      "without round-tripping through the host.");
  params.addRequiredParam<std::string>("from_moose", "Name of the MOOSE variable to gather");
  params.set<ExecFlagEnum>("execute_on") = {EXEC_INITIAL, EXEC_LINEAR, EXEC_NONLINEAR};
  return params;
}

template <typename T, unsigned int state>
KokkosQuantityToNEML2<T, state>::KokkosQuantityToNEML2(const InputParameters & params)
  : Moose::Kokkos::ElementUserObject(params),
    MOOSEToNEML2(params),
    _v(prepareVariableAndState<T, state>(
           this->_fe_problem, _tid, params.get<std::string>("from_moose")),
       stateTagName(state))
{
  if (state >= 3)
    mooseError("KokkosQuantityToNEML2 only supports state <= 2 (current, old, older).");

  NEML2Utils::assertNEML2Enabled();

  auto & var = getMooseVariable<T>(this->_fe_problem, _tid, params.get<std::string>("from_moose"));

  addFEVariableCoupleableVectorTag(_fe_problem.getVectorTagID(stateTagName(state)));
  addMooseVariableDependency(&var);
}

template <typename T, unsigned int state>
KokkosQuantityToNEML2<T, state>::KokkosQuantityToNEML2(const KokkosQuantityToNEML2 & object)
  : Moose::Kokkos::ElementUserObject(object),
    MOOSEToNEML2(object),
    _v(object._v),
    _buffer(object._buffer),
    _element_batch_offset(object._element_batch_offset)
{
}

template <typename T, unsigned int state>
void
KokkosQuantityToNEML2<T, state>::initialize()
{
  // Rebuild the element-major, quadrature-point-minor batch offsets on host. Ascending contiguous
  // element IDs match both the batch ordering produced by the host-side MOOSEQuantityToNEML2
  // gatherer and the row ordering consumed by NEML2ToKokkosMaterialProperty. The offsets are cheap
  // to recompute, so they are refreshed on every pass to pick up a mesh change without a
  // dedicated meshChanged() notification.
  const auto num_elems = kokkosMesh().getNumLocalElements();

  bool reallocated = false;
  if (_element_batch_offset.size() != num_elems)
  {
    _element_batch_offset.create(num_elems);
    reallocated = true;
  }

  dof_id_type num_entries = 0;
  for (const auto local_elem_index : make_range(num_elems))
  {
    _element_batch_offset.hostData()[local_elem_index] = num_entries;
    num_entries += kokkosAssembly().getNumQpsHost(local_elem_index);
  }
  _element_batch_offset.copyToDevice();

  // No copyToDevice() is needed for the gather buffer because it is populated
  // directly on the device during execute().
  if (_buffer.size() != num_entries)
  {
    _buffer.create(num_entries);
    reallocated = true;
  }

  // The dispatcher copies this object (and therefore the Array views) when it is first built; drop
  // it if the views changed so the next compute rebuilds it against the new buffers.
  if (reallocated)
    _user_object_dispatcher.reset();
}

#ifdef NEML2_ENABLED
template <typename T, unsigned int state>
at::Tensor
KokkosQuantityToNEML2<T, state>::gatheredData() const
{
  mooseAssert(_buffer.isAlloc(),
              "The gather buffer must be created before gatheredData is called.");

  // The gathered element-major, quadrature-point-minor buffer is re-viewed as a two-dimensional
  // batch [num_elements, num_qps], matching the batch convention of the Kokkos NEML2 kernels (e.g.
  // TorchSmallStrain emits (nelem, nqp, ...) and NEML2 broadcasts every input to that batch). The
  // host-side MOOSE gatherers use a leading one-dimensional batch of the same element-major
  // QP-minor order, so the values are identical; only the shape re-interpretation differs.
  const auto num_elems = static_cast<int64_t>(_element_batch_offset.size());
  mooseAssert(num_elems > 0,
              "The element batch offsets must be built before gatheredData is called.");
  // The batch shape is only meaningful when every element has the same number of quadrature points.
  const auto num_qps = static_cast<int64_t>(_buffer.size() / num_elems);
  mooseAssert(num_qps > 0, "An element must have at least one quadrature point.");

  std::vector<int64_t> shape = {num_elems, num_qps};
  for (const auto s : NEML2Utils::Layout<T>::shape)
    shape.push_back(s);

#ifdef MOOSE_ENABLE_KOKKOS_GPU
#ifdef KOKKOS_ENABLE_CUDA
  // Create a CUDA tensor view sharing the device-resident gather buffer, so NEML2 executes on the
  // gathered data without a device-to-host transfer.
  const auto options =
      at::TensorOptions().dtype(at::kDouble).device(at::kCUDA, ExecSpace().cuda_device());
  return at::from_blob(_buffer.deviceData(), shape, options);
#else
  mooseError("KokkosQuantityToNEML2 supports GPU execution only with CUDA.");
#endif
#else
  // Kokkos on host: the kernel writes host-accessible memory, and NEML2 consumes it on the CPU.
  const auto options = at::TensorOptions().dtype(at::kDouble);
  return at::from_blob(_buffer.deviceData(), shape, options);
#endif
}
#endif

template class KokkosQuantityToNEML2<Real, 0>;
template class KokkosQuantityToNEML2<Real, 1>;
template class KokkosQuantityToNEML2<Real, 2>;

template class KokkosQuantityToNEML2<RealVectorValue, 0>;
template class KokkosQuantityToNEML2<RealVectorValue, 1>;
template class KokkosQuantityToNEML2<RealVectorValue, 2>;
