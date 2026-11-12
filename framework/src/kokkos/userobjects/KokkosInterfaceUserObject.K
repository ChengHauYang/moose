//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosInterfaceUserObject.h"

#include "FEProblemBase.h"

namespace Moose::Kokkos
{

InputParameters
InterfaceUserObject::validParams()
{
  auto params = UserObject::validParams();
  params += BoundaryRestrictableRequired::validParams();
  params += TwoMaterialPropertyInterface::validParams();
  params.addRelationshipManager("ElementSideNeighborLayers",
                                Moose::RelationshipManagerType::GEOMETRIC |
                                    Moose::RelationshipManagerType::ALGEBRAIC |
                                    Moose::RelationshipManagerType::COUPLING);
  return params;
}

InterfaceUserObject::InterfaceUserObject(const InputParameters & parameters)
  : UserObject(parameters),
    MeshHolder(*_fe_problem.mesh().getKokkosMesh()),
    AssemblyHolder(_fe_problem.kokkosAssembly()),
    FESystemHolder(_fe_problem.getKokkosFESystems()),
    BoundaryRestrictableRequired(this, false),
    CoupleableMooseVariableDependencyIntermediateInterface(this, false),
    TwoMaterialPropertyInterface(this, Moose::EMPTY_BLOCK_IDS, boundaryIDs())
{
  if (boundaryIDs().empty())
    paramError("boundary", "Kokkos interface user objects require a boundary restriction.");

  _fe_problem.mesh().getKokkosMesh()->setNeedsElementSideGeometry();

  _fe_problem.addKokkosMeshInitializationHook(
      std::bind(&InterfaceUserObject::acquireInterfaceFaces, this));
}

InterfaceUserObject::InterfaceUserObject(const InterfaceUserObject & object)
  : UserObject(object),
    MeshHolder(object),
    AssemblyHolder(object),
    FESystemHolder(object),
    BoundaryRestrictableRequired(object, FunctorCopy()),
    CoupleableMooseVariableDependencyIntermediateInterface(object, FunctorCopy()),
    TwoMaterialPropertyInterface(object, FunctorCopy()),
    _interface_faces(object._interface_faces),
    _neighbor_qp_map(object._neighbor_qp_map)
{
}

void
InterfaceUserObject::compute()
{
  computeUserObject();
}

void
InterfaceUserObject::computeUserObject()
{
  Policy policy(0, numUserObjectThreads());

  if (!_interface_user_object_dispatcher)
    _interface_user_object_dispatcher = DispatcherRegistry::build<InterfaceLoop>(this, type());

  _interface_user_object_dispatcher->parallelFor(policy);
}

void
InterfaceUserObject::acquireInterfaceFaces()
{
  const auto * const cache = _fe_problem.getKokkosInterfaceFaceCache(boundaryIDs(), this);
  _interface_faces = cache->faces();
  _neighbor_qp_map = cache->neighborQpMap();
  _interface_user_object_dispatcher.reset();
}

} // namespace Moose::Kokkos
