//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GeneralUserObject.h"

#include "libmesh/system.h"

/**
 * Makes the value of a nodal variable uniform on a boundary by adding libMesh DOF constraints that
 * set every DOF of the variable on the boundary equal to the DOF at one primary node.
 *
 * The constraints are applied by the DofMap, like periodic constraints, so they are exact and the
 * host and Kokkos assembly both condense the constrained rows onto the primary DOF. The primary
 * DOF keeps its own equation, so a load on the boundary (e.g. a NeumannBC) becomes a load on that
 * single DOF.
 */
class EqualValueBoundaryDofConstraint : public GeneralUserObject, public libMesh::System::Constraint
{
public:
  static InputParameters validParams();

  EqualValueBoundaryDofConstraint(const InputParameters & parameters);

  virtual void initialize() override {}
  virtual void execute() override {}
  virtual void finalize() override {}

  /// Called by libMesh::System::reinit_constraints() to add the constraint rows
  virtual void constrain() override;

protected:
  /// The mesh holding the boundaries
  MooseMesh & _mesh;

  /// The boundaries whose variable DOFs are made equal
  const std::vector<BoundaryID> _boundary_ids;

  /// The (system, variable number) of the variable constrained on each boundary
  std::vector<std::pair<libMesh::System *, unsigned int>> _variables;
};
