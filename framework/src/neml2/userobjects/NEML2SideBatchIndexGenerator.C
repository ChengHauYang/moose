//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NEML2SideBatchIndexGenerator.h"
#include "NEML2Utils.h"

registerMooseObject("MooseApp", NEML2SideBatchIndexGenerator);

InputParameters
NEML2SideBatchIndexGenerator::validParams()
{
  auto params = SideUserObject::validParams();
  params.addClassDescription("Generates the side to batch index map for MOOSEToNEML2 gatherers, "
                             "NEML2ToMOOSE retrievers, and the NEML2 executor on sidesets");

  // Since we use the NEML2 model to evaluate the residual AND the Jacobian at the same time, we
  // want to execute this user object only at execute_on = LINEAR (i.e. during residual evaluation).
  // The NONLINEAR exec flag below is for computing Jacobian during automatic scaling.
  ExecFlagEnum execute_options = MooseUtils::getDefaultExecFlagEnum();
  execute_options = {EXEC_INITIAL, EXEC_LINEAR, EXEC_NONLINEAR};
  params.set<ExecFlagEnum>("execute_on") = execute_options;

  return params;
}

NEML2SideBatchIndexGenerator::NEML2SideBatchIndexGenerator(const InputParameters & params)
  : SideUserObject(params), _outdated(true), _batch_index(0)
{
}

void
NEML2SideBatchIndexGenerator::meshChanged()
{
  _outdated = true;
}

void
NEML2SideBatchIndexGenerator::initialize()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  if (!_outdated)
    return;

  _side_to_batch_index.clear();
  _side_to_batch_index_cache = {ElemSide(libMesh::invalid_uint, libMesh::invalid_uint), 0};
  _batch_index = 0;
}

void
NEML2SideBatchIndexGenerator::execute()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  if (!_outdated)
    return;

  const ElemSide key{_current_elem->id(), _current_side};

  // Side batch indices are generated starting from 0 (same as volume batch)
  // NEML2ModelExecutor shifts them by the volume batch count when side and volume
  // batches are concatenated.
  if (_side_to_batch_index.find(key) != _side_to_batch_index.end())
    mooseError("Duplicate side batch index key for element ",
               _current_elem->id(),
               ", side ",
               _current_side,
               ".");
  _side_to_batch_index[key] = _batch_index;
  _batch_index += _qrule->n_points();
}

void
NEML2SideBatchIndexGenerator::threadJoin(const UserObject & uo)
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  if (!_outdated)
    return;

  const auto & m2n = static_cast<const NEML2SideBatchIndexGenerator &>(uo);

  // append and renumber maps
  for (const auto & [key, batch_index] : m2n._side_to_batch_index)
  {
    if (_side_to_batch_index.find(key) != _side_to_batch_index.end())
      mooseError("Duplicate side batch index key encountered during thread join for element ",
                 std::get<0>(key),
                 ", side ",
                 std::get<1>(key),
                 ".");
    _side_to_batch_index[key] = _batch_index + batch_index;
  }

  _batch_index += m2n._batch_index;
}

void
NEML2SideBatchIndexGenerator::finalize()
{
  _outdated = false;
}

std::size_t
NEML2SideBatchIndexGenerator::getBatchIndex(const ElemSide & key) const
{
  // return cached map lookup if applicable
  if (_side_to_batch_index_cache.first == key)
    return _side_to_batch_index_cache.second;

  // else, search the map
  const auto it = _side_to_batch_index.find(key);
  if (it == _side_to_batch_index.end())
    mooseError("No side batch index found for element id ",
               std::get<0>(key),
               ", side ",
               std::get<1>(key),
               ".");
  _side_to_batch_index_cache = *it;
  return it->second;
}
