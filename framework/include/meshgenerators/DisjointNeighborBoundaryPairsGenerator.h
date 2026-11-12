//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "MeshGenerator.h"

#include "libmesh/vector_value.h"

/**
 * Registers disjoint neighbor boundary pairs on an input mesh.
 */
class DisjointNeighborBoundaryPairsGenerator : public MeshGenerator
{
public:
  static InputParameters validParams();

  DisjointNeighborBoundaryPairsGenerator(const InputParameters & parameters);

  std::unique_ptr<MeshBase> generate() override;

protected:
  std::unique_ptr<MeshBase> & _input;

  /// Boundary name/id pairs to register as disjoint neighbors
  const std::vector<std::vector<BoundaryName>> _boundary_pairs;

  /// Translation from the first boundary in each pair to the second
  const std::vector<RealVectorValue> _translations;
};
