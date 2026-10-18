//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosADShiftedInterfaceKernel.h"

#include "BoundaryShortestDistanceToSurface.h"

namespace Moose::Kokkos
{

InputParameters
ADShiftedInterfaceKernel::validParams()
{
  auto params = ADInterfaceKernel::validParams();
  params.addRequiredParam<UserObjectName>(
      "sbm_distance_uo", "UserObject providing the surrogate-interface mapping data.");
  return params;
}

ADShiftedInterfaceKernel::ADShiftedInterfaceKernel(const InputParameters & parameters)
  : ADInterfaceKernel(parameters),
    _sbm_distance_uo(getUserObject<BoundaryShortestDistanceToSurface>("sbm_distance_uo"))
{
  _fe_problem.addKokkosMeshInitializationHook(
      std::bind(&ADShiftedInterfaceKernel::initializeSurrogateInterfaceMapping, this));
}

ADShiftedInterfaceKernel::ADShiftedInterfaceKernel(const ADShiftedInterfaceKernel & object)
  : ADInterfaceKernel(object),
    _sbm_distance_uo(object._sbm_distance_uo),
    _qp_offsets(object._qp_offsets),
    _surrogate_distance_vectors(object._surrogate_distance_vectors),
    _true_normals(object._true_normals)
{
}

void
ADShiftedInterfaceKernel::computeResidual()
{
  ensureSurrogateInterfaceMappingReady();
  ADInterfaceKernel::computeResidual();
}

void
ADShiftedInterfaceKernel::computeJacobian()
{
  ensureSurrogateInterfaceMappingReady();
  ADInterfaceKernel::computeJacobian();
}

void
ADShiftedInterfaceKernel::computeResidualAndJacobian()
{
  ensureSurrogateInterfaceMappingReady();
  ADInterfaceKernel::computeResidualAndJacobian();
}

void
ADShiftedInterfaceKernel::initializeSurrogateInterfaceMapping()
{
  const auto & mapping = _sbm_distance_uo.registerKokkosSurrogateInterfaceMapping(
      boundaryIDs(), kokkosInterfaceFaces());
  _qp_offsets = mapping.qp_offsets;
  _surrogate_distance_vectors = mapping.surrogate_distance_vectors;
  _true_normals = mapping.true_normals;
}

void
ADShiftedInterfaceKernel::ensureSurrogateInterfaceMappingReady() const
{
  if (!_sbm_distance_uo.kokkosSurrogateInterfaceMappingReady(boundaryIDs()))
    mooseError("Surrogate-interface mapping for '",
               name(),
               "' is unavailable. Ensure its 'sbm_distance_uo' executes before nonlinear "
               "assembly.");
}

} // namespace Moose::Kokkos
