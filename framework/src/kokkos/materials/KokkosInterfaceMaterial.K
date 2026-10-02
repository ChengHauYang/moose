//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosInterfaceMaterial.h"

#include "FEProblemBase.h"

namespace Moose::Kokkos
{

InputParameters
InterfaceMaterial::validParams()
{
  auto params = Material::validParams();

  params.addRequiredCoupledVar("variable", "The interface variable on the primary side of a face.");
  params.addRequiredCoupledVar("neighbor_var",
                               "The interface variable on the neighbor side of a face.");
  params.addRelationshipManager("ElementSideNeighborLayers",
                                Moose::RelationshipManagerType::GEOMETRIC |
                                    Moose::RelationshipManagerType::ALGEBRAIC |
                                    Moose::RelationshipManagerType::COUPLING);

  return params;
}

InterfaceMaterial::InterfaceMaterial(const InputParameters & parameters)
  : Material(parameters),
    _variable_value(*getVarHelper<MooseVariableFE<Real>>("variable", 0)),
    _neighbor_variable_value(*getVarHelper<MooseVariableFE<Real>>("neighbor_var", 0))
{
  if (boundaryIDs().empty())
    paramError("boundary", "Kokkos interface materials require a boundary restriction.");
  if (!blockIDs().empty())
    paramError("block", "Kokkos interface materials cannot be restricted to blocks.");

  _fe_problem.addKokkosMeshInitializationHook(
      std::bind(&InterfaceMaterial::acquireInterfaceFaces, this));
}

InterfaceMaterial::InterfaceMaterial(const InterfaceMaterial & object)
  : Material(object),
    _variable_value(object._variable_value),
    _neighbor_variable_value(object._neighbor_variable_value),
    _interface_faces(object._interface_faces),
    _neighbor_qp_map(object._neighbor_qp_map)
{
}

void
InterfaceMaterial::initStatefulProperties(unsigned int)
{
  if (!DispatcherRegistry::hasUserMethod<InterfaceInit>(type()))
    return;

  Policy policy(0, numKokkosInterfaceFaces());

  if (!_init_dispatcher)
    _init_dispatcher = DispatcherRegistry::build<InterfaceInit>(this, type());

  _init_dispatcher->parallelFor(policy);
}

void
InterfaceMaterial::computeProperties()
{
  Policy policy(0, numKokkosInterfaceFaces());

  if (!_compute_dispatcher)
    _compute_dispatcher = DispatcherRegistry::build<InterfaceCompute>(this, type());

  _compute_dispatcher->parallelFor(policy);
}

void
InterfaceMaterial::acquireInterfaceFaces()
{
  const auto * const cache = _fe_problem.getKokkosInterfaceFaceCache(boundaryIDs(), this);
  _interface_faces = cache->faces();
  _neighbor_qp_map = cache->neighborQpMap();
}

} // namespace Moose::Kokkos
