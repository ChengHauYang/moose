//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "EqualValueBoundaryDofConstraint.h"
#include "FEProblemBase.h"
#include "MooseMesh.h"
#include "MooseVariableFieldBase.h"
#include "SystemBase.h"

#include "libmesh/boundary_info.h"
#include "libmesh/dof_map.h"

registerMooseObject("MooseApp", EqualValueBoundaryDofConstraint);

InputParameters
EqualValueBoundaryDofConstraint::validParams()
{
  InputParameters params = GeneralUserObject::validParams();
  params.addClassDescription(
      "Makes a nodal variable uniform on each listed boundary by constraining all of its degrees "
      "of freedom on the boundary to equal the one at the lowest-ID boundary node. The "
      "constraints are exact libMesh DOF constraints, applied by both host and Kokkos assembly.");
  params.addRequiredParam<std::vector<BoundaryName>>(
      "boundary", "The boundaries on which the variables are made uniform");
  params.addRequiredParam<std::vector<VariableName>>(
      "variable",
      "The scalar Lagrange variable made uniform on each boundary, paired with 'boundary' by "
      "position. A single variable applies to every boundary.");

  // The constraints are built by libMesh when the DOF map is (re)initialized
  params.set<ExecFlagEnum>("execute_on") = EXEC_NONE;
  params.suppressParameter<ExecFlagEnum>("execute_on");
  return params;
}

EqualValueBoundaryDofConstraint::EqualValueBoundaryDofConstraint(const InputParameters & parameters)
  : GeneralUserObject(parameters),
    _mesh(_fe_problem.mesh()),
    _boundary_ids(_mesh.getBoundaryIDs(getParam<std::vector<BoundaryName>>("boundary")))
{
  const auto & var_names = getParam<std::vector<VariableName>>("variable");
  if (var_names.size() != 1 && var_names.size() != _boundary_ids.size())
    paramError("variable", "Provide either one variable or one variable per boundary.");

  for (const auto i : index_range(_boundary_ids))
  {
    const auto & name = var_names[var_names.size() == 1 ? 0 : i];
    if (!_fe_problem.hasVariable(name))
      paramError("variable", "Variable '", name, "' does not exist.");
    auto & var = _fe_problem.getVariable(0, name);
    if (var.feType().family != LAGRANGE || var.count() != 1)
      paramError("variable", "Variable '", name, "' must be a scalar Lagrange variable.");
    _variables.emplace_back(&var.sys().system(), var.number());
  }

  // libMesh supports one constraint object per system
  std::set<libMesh::System *> systems;
  for (const auto & var : _variables)
    systems.insert(var.first);
  for (auto * const sys : systems)
  {
    if (sys->has_constraint_object())
      mooseError("System '",
                 sys->name(),
                 "' already has a libMesh constraint object. Only one ",
                 type(),
                 " may act on a system; list all of its boundaries in one object.");
    sys->attach_constraint_object(*this);
  }
}

void
EqualValueBoundaryDofConstraint::constrain()
{
  const auto & mesh = _mesh.getMesh();
  const auto node_list =
      mesh.get_boundary_info().build_node_list(libMesh::BoundaryInfo::NodeBCTupleSortBy::UNSORTED);

  for (const auto i : index_range(_boundary_ids))
  {
    const auto bnd_id = _boundary_ids[i];
    auto & [sys, var] = _variables[i];
    const auto sys_num = sys->number();

    // Boundary nodes this processor knows about (local and ghosted) that carry the variable
    std::vector<const Node *> nodes;
    for (const auto & [node_id, node_bnd_id] : node_list)
      if (node_bnd_id == bnd_id)
      {
        const Node * const node = mesh.node_ptr(node_id);
        if (node->n_comp(sys_num, var))
          nodes.push_back(node);
      }

    // The primary node is the lowest-ID boundary node; its owner provides its DOF index
    dof_id_type primary_id = DofObject::invalid_id;
    for (const auto * const node : nodes)
      primary_id = std::min(primary_id, node->id());
    _communicator.min(primary_id);
    if (primary_id == DofObject::invalid_id)
      paramError("boundary",
                 "Boundary '",
                 _mesh.getBoundaryName(bnd_id),
                 "' has no nodes with degrees of freedom of the constrained variable.");

    dof_id_type primary_dof = DofObject::invalid_id;
    if (const Node * const primary = mesh.query_node_ptr(primary_id);
        primary && primary->processor_id() == processor_id())
      primary_dof = primary->dof_number(sys_num, var, 0);
    _communicator.min(primary_dof);

    auto & dof_map = sys->get_dof_map();
    for (const auto * const node : nodes)
    {
      const auto dof = node->dof_number(sys_num, var, 0);
      if (dof == primary_dof)
        continue;
      libMesh::DofConstraintRow row;
      row[primary_dof] = 1.0;
      dof_map.add_constraint_row(dof, row, 0.0, /*forbid_constraint_overwrite=*/false);
    }
  }
}
