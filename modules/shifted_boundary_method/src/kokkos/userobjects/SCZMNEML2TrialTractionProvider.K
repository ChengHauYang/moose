//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SCZMNEML2TrialTractionProvider.h"

#ifdef NEML2_ENABLED

#include "MooseException.h"
#include "NEML2BatchIndexGenerator.h"
#include "NEML2ModelExecutor.h"

#include <algorithm>

namespace
{
using Moose::Kokkos::SCZM::CohesiveEvaluation;
using Moose::Kokkos::SCZM::GeneralizedRobinSolver;

/// The NEML2 model is evaluated for the whole batch outside the evaluator, which therefore only
/// maps jumps and tractions between the local and global frames and never calls its local law
struct BatchEvaluatedLocalLaw
{
};
using FrameMap = Moose::Kokkos::SCZM::GlobalTractionTotalLagrangian<BatchEvaluatedLocalLaw>;
}

SCZMNEML2TrialTractionProvider::SCZMNEML2TrialTractionProvider(
    NEML2ModelExecutor & executor,
    const NEML2BatchIndexGenerator & generator,
    const std::string & jump_input,
    const std::string & traction_output)
  : _executor(executor),
    _generator(generator),
    _jump_input(jump_input),
    _traction_output(traction_output),
    _has_history(executor.hasHistory())
{
}

void
SCZMNEML2TrialTractionProvider::checkStabilizationLaw(const KokkosSCZMStabilizationData & uo) const
{
  if (uo.coupledCubicStabilization())
    mooseError("The weighted traction stabilization law of '",
               uo.name(),
               "' must be 'affine' with the NEML2 cohesive model of '",
               _executor.name(),
               "', which supplies the trial law itself.");
  // Without on-device state advance, the old state is read back from material properties that
  // were computed at the shifted jump before the generalized Robin solve
  if (_has_history && !_executor.getParam<bool>("manage_state_advance"))
    mooseError("Weighted traction stabilization on '",
               uo.name(),
               "' requires 'manage_state_advance = true' for the NEML2 cohesive model of '",
               _executor.name(),
               "', which carries history: the state must be advanced from the auxiliary jump of "
               "the generalized Robin solve.");
}

void
SCZMNEML2TrialTractionProvider::updateRowMap(const KokkosSCZMStabilizationData & uo) const
{
  const auto generation = _generator.mapGeneration();
  if (generation != 0 && generation == _map_generation)
    return;
  if (generation == 0)
    mooseError("Weighted traction stabilization on '",
               uo.name(),
               "' found no batch for the NEML2 cohesive model of '",
               _executor.name(),
               "'.");

  const auto invalid = static_cast<std::size_t>(libMesh::DofObject::invalid_id);
  const auto batch_size = _generator.getBatchIndex();
  _primary_rows.assign(uo.numQps(), invalid);
  _neighbor_rows.assign(uo.numQps(), invalid);
  std::vector<bool> row_used(batch_size, false);
  const auto claim = [&](const NEML2BatchIndexGenerator::BatchSpan & span,
                         const Moose::Kokkos::InterfaceFaceRecord & face,
                         std::vector<std::size_t> & rows)
  {
    if (span.size != face.n_qps || span.begin + span.size > batch_size)
      mooseError("The NEML2 batch of '",
                 _executor.name(),
                 "' has ",
                 span.size,
                 " rows for interface element ",
                 face.elem_id,
                 " side ",
                 face.side,
                 ", which has ",
                 face.n_qps,
                 " weighted traction stabilization quadrature points.");
    for (const auto qp : make_range(face.n_qps))
    {
      if (row_used[span.begin + qp])
        mooseError("The NEML2 batch of '",
                   _executor.name(),
                   "' maps two weighted traction stabilization quadrature points to row ",
                   span.begin + qp,
                   ".");
      row_used[span.begin + qp] = true;
      rows[face.qp_map_offset + qp] = span.begin + qp;
    }
  };

  for (const auto & face : uo.interfaceFaces())
  {
    const NEML2BatchIndexGenerator::ElemSide primary(face.elem_id, face.side);
    if (!_executor.hasSideBatchSpan(primary))
      mooseError("The NEML2 cohesive model of '",
                 _executor.name(),
                 "' is not evaluated on interface element ",
                 face.elem_id,
                 " side ",
                 face.side,
                 " of the weighted traction stabilization '",
                 uo.name(),
                 "'.");
    claim(_executor.getSideBatchSpan(primary), face, _primary_rows);

    // The gatherer copies the primary jump into a batched neighbor side, so the trial does too
    const NEML2BatchIndexGenerator::ElemSide neighbor(face.neighbor_id, face.neighbor_side);
    if (_executor.hasSideBatchSpan(neighbor))
      claim(_executor.getSideBatchSpan(neighbor), face, _neighbor_rows);
  }
  _map_generation = generation;
}

void
SCZMNEML2TrialTractionProvider::robinPrepass(const KokkosSCZMStabilizationData & uo) const
{
  const auto n_qps = uo.numQps();
  // A rank owning no interface face has nothing to solve and does not evaluate the model
  if (n_qps == 0)
    return;

  updateRowMap(uo);
  uo.gatherRobinInputs(_inputs);

  const auto ndisp = uo.numDisplacements();
  std::vector<FrameMap> frames(n_qps);
  std::vector<GeneralizedRobinSolver::PointState> states(n_qps);
  std::vector<dof_id_type> active;
  for (const auto i : make_range(n_qps))
  {
    auto & frame = frames[i];
    frame.total_rotation = _inputs.total_rotation[i];
    frame.reference_rotation = _inputs.reference_rotation[i];
    frame.rotation = _inputs.rotation[i];
    frame.deformation_gradient = _inputs.deformation_gradient[i];
    frame.reference_normal = _inputs.reference_normal[i];
    frame.ndisp = ndisp;
    // Whether the model is affine is unknown, so every point verifies its Newton update
    states[i] = GeneralizedRobinSolver::begin(
        _inputs.shifted_jump[i], _inputs.bulk_traction[i], _inputs.beta[i], ndisp, false);
    if (states[i].status == GeneralizedRobinSolver::PointStatus::Active)
      active.push_back(i);
  }

  // Local evaluation last consumed by each point; at convergence it is the one at the auxiliary
  // jump, which the deformation-gradient derivative needs
  std::vector<CohesiveEvaluation> local_evaluations(n_qps);
  // Rows of no stabilization QP keep a zero jump; their outputs are never read. Rows of terminal
  // points keep their last jump.
  auto local_jump = at::zeros({static_cast<int64_t>(_generator.getBatchIndex()), 3},
                              at::TensorOptions().dtype(at::kDouble));
  auto jump = local_jump.accessor<double, 2>();
  const auto invalid = static_cast<std::size_t>(libMesh::DofObject::invalid_id);
  bool batch_failed = false;
  try
  {
    while (!active.empty())
    {
      for (const auto i : active)
      {
        const auto local = frames[i].toLocalJump(states[i].requested_jump, ndisp);
        for (const auto component : make_range(3))
        {
          jump[_primary_rows[i]][component] = local(component);
          if (_neighbor_rows[i] != invalid)
            jump[_neighbor_rows[i]][component] = local(component);
        }
      }

      const auto [traction, tangent] =
          _executor.evaluateTrial(_jump_input, _traction_output, local_jump);
      const auto traction_rows = traction.accessor<double, 2>();
      const auto tangent_rows = tangent.accessor<double, 3>();
      for (const auto i : active)
      {
        auto & local = local_evaluations[i];
        const auto row = _primary_rows[i];
        for (const auto a : make_range(3))
        {
          local.traction(a) = traction_rows[row][a];
          for (const auto b : make_range(3))
            local.jump_tangent(a, b) = tangent_rows[row][a][b];
        }
        GeneralizedRobinSolver::consume(states[i], frames[i].toGlobal(local, ndisp));
      }
      active.erase(std::remove_if(
                       active.begin(),
                       active.end(),
                       [&](const auto i)
                       { return states[i].status != GeneralizedRobinSolver::PointStatus::Active; }),
                   active.end());
    }
  }
  catch (const MooseException &)
  {
    // A recoverable NEML2 failure has no per-point status, so the whole batch fails
    batch_failed = true;
  }

  // A model with history advances its state from the jump at which the law acts
  const bool all_converged =
      std::all_of(states.begin(),
                  states.end(),
                  [](const auto & state)
                  { return state.status == GeneralizedRobinSolver::PointStatus::Converged; });
  if (_has_history && !batch_failed && all_converged)
  {
    auto replace_rows = at::zeros({static_cast<int64_t>(_generator.getBatchIndex())},
                                  at::TensorOptions().dtype(at::kBool));
    auto replace = replace_rows.accessor<bool, 1>();
    for (const auto i : make_range(n_qps))
    {
      const auto local = frames[i].toLocalJump(states[i].result.auxiliary_jump, ndisp);
      for (const auto row : {_primary_rows[i], _neighbor_rows[i]})
      {
        if (row == invalid)
          continue;
        replace[row] = true;
        for (const auto component : make_range(3))
          jump[row][component] = local(component);
      }
    }
    try
    {
      _executor.publishTrial(_jump_input, local_jump, replace_rows);
    }
    catch (const MooseException &)
    {
      batch_failed = true;
    }
  }

  const bool finite_strain = uo.finiteStrain();
  if (_results.traction.size() != n_qps)
  {
    _results.traction.create(n_qps);
    _results.auxiliary_jump.create(n_qps);
    _results.robin_W.create(n_qps);
    _results.inverse_robin_tangent.create(n_qps);
    _results.converged.create(n_qps);
    if (finite_strain)
    {
      // 27 = 3 x 3 x 3 entries (a, k, l); entries beyond ndisp stay zero
      _results.robin_weighted_dF.create(n_qps, 27);
      _results.robin_weighted_dF = 0.0;
    }
  }

  for (const auto i : make_range(n_qps))
  {
    const auto & result = states[i].result;
    const bool converged =
        !batch_failed && states[i].status == GeneralizedRobinSolver::PointStatus::Converged;
    _results.converged[i] = converged;
    if (!converged)
      continue;
    _results.traction[i] = result.traction;
    _results.auxiliary_jump[i] = result.auxiliary_jump;
    _results.robin_W[i] = result.dtraction_djump;
    _results.inverse_robin_tangent[i] = result.inverse_robin_tangent;
    if (!finite_strain)
      continue;
    const auto auxiliary =
        frames[i].toGlobalWithDF(result.auxiliary_jump, local_evaluations[i], ndisp);
    for (const auto a : make_range(ndisp))
      for (const auto k : make_range(ndisp))
        for (const auto l : make_range(ndisp))
        {
          Real weighted = 0.0;
          for (const auto b : make_range(ndisp))
            weighted += result.inverse_robin_tangent(a, b) * auxiliary.dtraction_dF[b][k][l];
          _results.robin_weighted_dF(i, 9 * a + 3 * k + l) = weighted;
        }
  }

  _results.traction.copyToDevice();
  _results.auxiliary_jump.copyToDevice();
  _results.robin_W.copyToDevice();
  _results.inverse_robin_tangent.copyToDevice();
  _results.converged.copyToDevice();
  if (finite_strain)
    _results.robin_weighted_dF.copyToDevice();
  uo.storeRobinResults(_results);
}

#endif
