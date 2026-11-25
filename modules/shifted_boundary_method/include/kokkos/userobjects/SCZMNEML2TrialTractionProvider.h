//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosSCZMStabilizationData.h"

#ifdef NEML2_ENABLED

class NEML2BatchIndexGenerator;
class NEML2ModelExecutor;

/**
 * Evaluates a NEML2 cohesive model as the weighted-traction trial law.
 *
 * The model maps a local jump to a local traction. The generalized Robin solve runs on the host:
 * every iteration replaces the model's jump input for the whole batch and evaluates it once with
 * NEML2ModelExecutor::evaluateTrial(), keeping all other inputs as last gathered by the executor.
 * The executor's published outputs are left untouched, except for a model with history: the law
 * acts at the auxiliary jump, so once every point converges the model is re-evaluated there with
 * NEML2ModelExecutor::publishTrial(), and the executor saves that state at the end of the step.
 */
class SCZMNEML2TrialTractionProvider : public SCZMTrialTractionProvider
{
public:
  SCZMNEML2TrialTractionProvider(NEML2ModelExecutor & executor,
                                 const NEML2BatchIndexGenerator & generator,
                                 const std::string & jump_input,
                                 const std::string & traction_output);

  void robinPrepass(const KokkosSCZMStabilizationData & uo) const override;
  void checkStabilizationLaw(const KokkosSCZMStabilizationData & uo) const override;

private:
  /// Rebuilds the stabilization-QP-to-batch-row map whenever the generator rebuilds its batch
  void updateRowMap(const KokkosSCZMStabilizationData & uo) const;

  /// Publishes the evaluation at the auxiliary jump of a model with history
  NEML2ModelExecutor & _executor;
  const NEML2BatchIndexGenerator & _generator;
  /// NEML2 input variable holding the local cohesive jump
  const std::string _jump_input;
  /// NEML2 output variable holding the local cohesive traction
  const std::string _traction_output;
  /// Whether the model carries history, whose state must come from the auxiliary jump
  const bool _has_history;

  /// Primary-side batch row of each stabilization QP, in interface-face/QP order
  mutable std::vector<std::size_t> _primary_rows;
  /// Neighbor-side batch row of each stabilization QP, or invalid_id when that side is not batched
  mutable std::vector<std::size_t> _neighbor_rows;
  /// Generator map generation the row map was built for; zero before the generator has run
  mutable std::size_t _map_generation = 0;

  mutable KokkosSCZMStabilizationData::RobinInputs _inputs;
  mutable KokkosSCZMStabilizationData::RobinResults _results;
};

#endif
