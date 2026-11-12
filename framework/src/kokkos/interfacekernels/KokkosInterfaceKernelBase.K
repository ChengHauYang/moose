//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosInterfaceKernelBase.h"

#include "FEProblemBase.h"

namespace Moose::Kokkos
{

InputParameters
InterfaceKernelBase::validParams()
{
  auto params = ResidualObject::validParams();
  params += BoundaryRestrictableRequired::validParams();
  params += TwoMaterialPropertyInterface::validParams();
  params += LocalParallelInterface::validParams();

  params.addRequiredCoupledVar("neighbor_var", "The variable on the other side of the interface.");
  params.registerBase("InterfaceKernel");
  params.registerSystemAttributeName("InterfaceKernel");
  params.addRelationshipManager("ElementSideNeighborLayers",
                                Moose::RelationshipManagerType::GEOMETRIC |
                                    Moose::RelationshipManagerType::ALGEBRAIC |
                                    Moose::RelationshipManagerType::COUPLING);

  return params;
}

InterfaceKernelBase::InterfaceKernelBase(const InputParameters & parameters)
  : ResidualObject(parameters, Moose::VarFieldType::VAR_FIELD_STANDARD),
    BoundaryRestrictableRequired(this, false),
    CoupleableMooseVariableDependencyIntermediateInterface(this, false),
    TwoMaterialPropertyInterface(this, Moose::EMPTY_BLOCK_IDS, boundaryIDs()),
    LocalParallelInterface(this),
    _neighbor_var(*getVarHelper<MooseVariableFE<Real>>("neighbor_var", 0)),
    _kokkos_neighbor_var(_neighbor_var)
{
  addMooseVariableDependency(&_var);
  addMooseVariableDependency(&_neighbor_var);

  if (_var.sys().number() != _neighbor_var.sys().number())
    paramError("neighbor_var", "Kokkos interface variables must belong to the same system.");

  _fe_problem.addKokkosMeshInitializationHook(
      std::bind(&InterfaceKernelBase::acquireInterfaceFaces, this));
}

InterfaceKernelBase::InterfaceKernelBase(const InterfaceKernelBase & object)
  : ResidualObject(object),
    BoundaryRestrictableRequired(object, FunctorCopy()),
    CoupleableMooseVariableDependencyIntermediateInterface(object, FunctorCopy()),
    TwoMaterialPropertyInterface(object, FunctorCopy()),
    LocalParallelInterface(object),
    _neighbor_var(object._neighbor_var),
    _kokkos_neighbor_var(object._kokkos_neighbor_var),
    _interface_faces(object._interface_faces),
    _neighbor_qp_map(object._neighbor_qp_map)
{
}

void
InterfaceKernelBase::acquireInterfaceFaces()
{
  const auto * const cache = _fe_problem.getKokkosInterfaceFaceCache(boundaryIDs(), this);
  _interface_faces = cache->faces();
  _neighbor_qp_map = cache->neighborQpMap();
}

} // namespace Moose::Kokkos
