//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosInterfaceMaterial.h"

#include "FEProblemBase.h"

namespace Moose::Kokkos
{

InputParameters
InterfaceMaterial::validParams()
{
  auto params = Material::validParams();

  params.addCoupledVar("variable", "The interface variable on the primary side of a face.");
  params.addCoupledVar("neighbor_var", "The interface variable on the neighbor side of a face.");
  params.addRelationshipManager("ElementSideNeighborLayers",
                                Moose::RelationshipManagerType::GEOMETRIC |
                                    Moose::RelationshipManagerType::ALGEBRAIC |
                                    Moose::RelationshipManagerType::COUPLING);

  return params;
}

InterfaceMaterial::InterfaceMaterial(const InputParameters & parameters)
  : Material(parameters),
    _variable_value(isCoupled("variable") ? kokkosCoupledValue("variable") : VariableValue()),
    _neighbor_variable_value(isCoupled("neighbor_var") ? kokkosCoupledValue("neighbor_var")
                                                       : VariableValue())
{
  if (boundaryIDs().empty())
    paramError("boundary", "Kokkos interface materials require a boundary restriction.");
  if (!blockIDs().empty())
    paramError("block", "Kokkos interface materials cannot be restricted to blocks.");

  // InterfaceMaterial currently does not implement constant_on semantics; reject
  // non-NONE to prevent incorrect single-QP evaluation or data races.
  const auto constant_option = getParam<MooseEnum>("constant_on");
  if (constant_option.getEnum<PropertyConstantOption>() != PropertyConstantOption::NONE)
    paramError("constant_on",
               "Kokkos interface materials currently only support constant_on = NONE.");

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
