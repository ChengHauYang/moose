//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#ifdef NEML2_ENABLED

#include "HomogenizationInterface.h"
#include "MooseVariableScalar.h"
#include "SystemBase.h"

#include <torch/torch.h>

namespace Homogenization
{
/**
 * Tensor indices (i, j) of the constraints that are not none, in the order of the constraint map
 * built by HomogenizationInterface, which is the order of the macro gradient components
 */
inline std::vector<std::pair<unsigned int, unsigned int>>
constraintIndices(const MultiMooseEnum & types)
{
  std::map<std::pair<unsigned int, unsigned int>, bool> indices;
  for (const auto j : make_range(Moose::dim))
    for (const auto i : make_range(Moose::dim))
      if (static_cast<ConstraintType>(types.get(i + Moose::dim * j)) != ConstraintType::None)
        indices[{i, j}] = true;

  std::vector<std::pair<unsigned int, unsigned int>> result;
  for (const auto & [ij, _] : indices)
    result.push_back(ij);
  return result;
}

/**
 * Assemble the current value of the macro gradient scalar variable into a (3, 3) tensor
 * @param var The macro gradient scalar variable
 * @param indices The tensor indices of the macro gradient components
 * @param options The options (device and dtype) of the returned tensor
 */
inline at::Tensor
macroGradient(const MooseVariableScalar & var,
              const std::vector<std::pair<unsigned int, unsigned int>> & indices,
              const at::TensorOptions & options)
{
  std::vector<dof_id_type> dofs;
  var.sys().dofMap().SCALAR_dof_indices(dofs, var.number());
  const auto & solution = *var.sys().currentSolution();

  auto H = at::zeros({3, 3}, at::TensorOptions().dtype(at::kDouble));
  auto H_a = H.accessor<double, 2>();
  for (const auto h : index_range(indices))
    H_a[indices[h].first][indices[h].second] = solution(dofs[h]);

  return H.to(options);
}
}

#endif // NEML2_ENABLED
