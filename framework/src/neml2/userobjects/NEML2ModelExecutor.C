//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NEML2ModelExecutor.h"
#include "MOOSEToNEML2.h"
#include "NEML2Utils.h"
#include <string>

#ifdef NEML2_ENABLED
#include <torch/torch.h>
#include <cstring>
#include "libmesh/id_types.h"
#include "neml2/tensors/functions/jacrev.h"
#include "neml2/tensors/functions/cat.h"
#include "neml2/dispatchers/ValueMapLoader.h"
#endif

registerMooseObject("MooseApp", NEML2ModelExecutor);

#ifdef NEML2_ENABLED
namespace
{
std::size_t
tensorCompSize(const neml2::Tensor & t, std::size_t batch)
{
  const auto numel = static_cast<std::size_t>(t.numel());
  return batch > 0 ? numel / batch : 0;
}

std::vector<Real>
packElementData(const neml2::Tensor & t,
                const std::vector<dof_id_type> & elem_ids,
                const std::map<dof_id_type, std::size_t> & elem_to_start,
                const std::map<dof_id_type, unsigned int> & elem_to_nqp)
{
  std::vector<Real> packed;
  if (!t.defined())
    return packed;

  auto cpu = t.to(at::kCPU).contiguous();
  const auto batch = static_cast<std::size_t>(cpu.size(0));
  const auto comp = tensorCompSize(cpu, batch);
  if (comp == 0)
    return packed;

  std::size_t total_qp = 0;
  for (const auto & elem_id : elem_ids)
    total_qp += elem_to_nqp.at(elem_id);
  packed.reserve(total_qp * comp);

  const auto * ptr = cpu.data_ptr<Real>();
  for (const auto & elem_id : elem_ids)
  {
    const auto start = elem_to_start.at(elem_id);
    const auto nqp = elem_to_nqp.at(elem_id);
    for (unsigned int qp = 0; qp < nqp; ++qp)
    {
      const auto offset = (start + qp) * comp;
      packed.insert(packed.end(), ptr + offset, ptr + offset + comp);
    }
  }

  return packed;
}
} // namespace
#endif

InputParameters
NEML2ModelExecutor::actionParams()
{
  auto params = emptyInputParameters();
  // allow user to explicit skip required input variables
  params.addParam<std::vector<std::string>>(
      "skip_inputs",
      {},
      "List of NEML2 variables to skip error checking when setting up the model input. If an "
      "input variable is skipped, its value will stay zero. If a required input variable is "
      "not skipped, an error will be raised.");
  return params;
}

InputParameters
NEML2ModelExecutor::validParams()
{
  auto params = NEML2ModelInterface<GeneralUserObject>::validParams();
  params += NEML2ModelExecutor::actionParams();
  params.addClassDescription("Execute the specified NEML2 model");

  params.addRequiredParam<UserObjectName>(
      "batch_index_generator",
      "The NEML2BatchIndexGenerator used to generate the element-to-batch-index map.");
  params.addParam<UserObjectName>(
      "side_batch_index_generator",
      "",
      "The NEML2SideBatchIndexGenerator used to generate the side-to-batch-index map.");
  params.addParam<std::vector<UserObjectName>>(
      "gatherers",
      {},
      "List of MOOSE*ToNEML2 user objects gathering MOOSE data as NEML2 input variables");
  params.addParam<std::vector<UserObjectName>>(
      "side_gatherers",
      {},
      "List of MOOSE*ToNEML2 user objects gathering MOOSE side data as NEML2 input variables");
  params.addParam<std::vector<UserObjectName>>(
      "param_gatherers",
      {},
      "List of MOOSE*ToNEML2 user objects gathering MOOSE data as NEML2 model parameters");

  // Since we use the NEML2 model to evaluate the residual AND the Jacobian at the same time, we
  // want to execute this user object only at execute_on = LINEAR (i.e. during residual evaluation).
  // The NONLINEAR exec flag below is for computing Jacobian during automatic scaling.
  ExecFlagEnum execute_options = MooseUtils::getDefaultExecFlagEnum();
  execute_options = {EXEC_INITIAL, EXEC_LINEAR, EXEC_NONLINEAR};
  params.set<ExecFlagEnum>("execute_on") = execute_options;

  return params;
}

NEML2ModelExecutor::NEML2ModelExecutor(const InputParameters & params)
  : NEML2ModelInterface<GeneralUserObject>(params)
#ifdef NEML2_ENABLED
    ,
    _batch_index_generator(getUserObject<NEML2BatchIndexGenerator>("batch_index_generator")),
    _output_ready(false),
    _error_message("")
#endif
{
#ifdef NEML2_ENABLED
  validateModel();

  // add user object dependencies by name (the UOs do not need to exist yet for this)
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("gatherers"))
    _depend_uo.insert(gatherer_name);
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("side_gatherers"))
    _depend_uo.insert(gatherer_name);
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("param_gatherers"))
    _depend_uo.insert(gatherer_name);
  const auto & side_idx_name = getParam<UserObjectName>("side_batch_index_generator");
  if (!side_idx_name.empty())
  {
    _depend_uo.insert(side_idx_name);
    _side_batch_index_generator =
        &getUserObject<NEML2SideBatchIndexGenerator>("side_batch_index_generator");
  }

  // variables to skip error checking (converting vector to set to prevent duplicate checks)
  for (const auto & var_name : getParam<std::vector<std::string>>("skip_inputs"))
    _skip_vars.insert(NEML2Utils::parseVariableName(var_name));
#endif
}

#ifdef NEML2_ENABLED
void
NEML2ModelExecutor::initialSetup()
{
  // deal with user object provided inputs
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("gatherers"))
  {
    // gather coupled user objects late to ensure they are constructed. Do not add them as
    // dependencies (that's already done in the constructor).
    const auto & uo = getUserObjectByName<MOOSEToNEML2>(gatherer_name, /*is_dependency=*/false);

    // the target neml2 variable must exist on the input axis
    if (!model().input_axis().has_variable(NEML2Utils::parseVariableName(uo.NEML2Name())))
      mooseError("The MOOSEToNEML2 gatherer named '",
                 gatherer_name,
                 "' is gathering MOOSE data for a non-existent NEML2 input variable named '",
                 uo.NEML2Name(),
                 "'.");

    // tell the gatherer to gather for a model input variable
    const auto varname = NEML2Utils::parseVariableName(uo.NEML2Name());
    if (varname.is_old_force() || varname.is_old_state())
      uo.setMode(MOOSEToNEML2::Mode::OLD_VARIABLE);
    else
      uo.setMode(MOOSEToNEML2::Mode::VARIABLE);

    addGatheredVariable(gatherer_name, uo.NEML2VariableName());
    _gatherers.push_back(&uo);
  }

  // deal with user object provided side inputs
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("side_gatherers"))
  {
    const auto & uo = getUserObjectByName<MOOSEToNEML2>(gatherer_name, /*is_dependency=*/false);

    if (!model().input_axis().has_variable(NEML2Utils::parseVariableName(uo.NEML2Name())))
      mooseError("The MOOSEToNEML2 gatherer named '",
                 gatherer_name,
                 "' is gathering MOOSE data for a non-existent NEML2 input variable named '",
                 uo.NEML2Name(),
                 "'.");

    // side gatherers must target input variables, not model parameters
    if (model().named_parameters().count(uo.NEML2Name()) == 1)
      mooseError("The side MOOSEToNEML2 gatherer named '",
                 gatherer_name,
                 "' is gathering for a NEML2 model parameter named '",
                 uo.NEML2Name(),
                 "', which is not supported for side inputs.");

    const auto varname = NEML2Utils::parseVariableName(uo.NEML2Name());
    if (varname.is_old_force() || varname.is_old_state())
      uo.setMode(MOOSEToNEML2::Mode::OLD_VARIABLE);
    else
      uo.setMode(MOOSEToNEML2::Mode::VARIABLE);

    _side_gatherers.push_back(&uo);
  }

  // deal with user object provided model parameters
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("param_gatherers"))
  {
    // gather coupled user objects late to ensure they are constructed. Do not add them as
    // dependencies (that's already done in the constructor).
    const auto & uo = getUserObjectByName<MOOSEToNEML2>(gatherer_name, /*is_dependency=*/false);

    // introspect the NEML2 model to figure out if the gatherer UO is gathering for a NEML2 input
    // variable or for a NEML2 model parameter
    if (model().named_parameters().count(uo.NEML2Name()) != 1)
      mooseError("The MOOSEToNEML2 gatherer named '",
                 gatherer_name,
                 "' is gathering MOOSE data for a non-existent NEML2 model parameter named '",
                 uo.NEML2Name(),
                 "'.");

    // tell the gatherer to gather for a model parameter
    uo.setMode(MOOSEToNEML2::Mode::PARAMETER);

    addGatheredParameter(gatherer_name, uo.NEML2ParameterName());
    _gatherers.push_back(&uo);
  }

  // iterate over set of required inputs and error out if we find one that is not provided
  std::vector<neml2::VariableName> required_inputs = model().input_axis().variable_names();
  for (const auto & input : required_inputs)
  {
    if (_skip_vars.count(input))
      continue;
    // skip input state variables because they are "initial guesses" to the nonlinear system
    if (input.is_state())
      continue;
    if (!_gathered_variable_names.count(input))
      paramError("gatherers", "The required model input `", input, "` is not gathered");
  }

  // If a variable is stateful, then it'd better been retrieved by someone! In theory that's not
  // sufficient for stateful data management, but that's the best we can do here without being
  // overly restrictive.
  for (const auto & input : required_inputs)
    if (input.is_old_state() && !_retrieved_outputs.count(input.current()))
      mooseError(
          "The NEML2 model requires a stateful input variable `",
          input,
          "`, but its state counterpart on the output axis has not been retrieved by any object. "
          "Therefore, there is no way to properly propagate the corresponding stateful data in "
          "time. The common solution to this problem is to add a NEML2ToMOOSE retriever such as "
          "those called `NEML2To*MOOSEMaterialProperty`.");

  // check if the model has state/old_state
  for (const auto & [vname, var] : model().input_variables())
  {
    if (vname.is_state())
      _has_state = true;
    if (vname.is_old_state())
      _has_old_state = true;
  }
}

std::size_t
NEML2ModelExecutor::getBatchIndex(dof_id_type elem_id) const
{
  return _batch_index_generator.getBatchIndex(elem_id);
}

bool
NEML2ModelExecutor::hasLocalBatchIndex(dof_id_type elem_id) const
{
  const auto & local_map = _batch_index_generator.elemToBatchIndex();
  return local_map.find(elem_id) != local_map.end();
}

NEML2ModelExecutor::BatchInfo
NEML2ModelExecutor::getBatchInfo(dof_id_type elem_id) const
{
  const auto & local_map = _batch_index_generator.elemToBatchIndex();
  const auto & local_nqp = _batch_index_generator.elemToNQPs();
  if (auto it = local_map.find(elem_id); it != local_map.end())
  {
    const auto nqp_it = local_nqp.find(elem_id);
    if (nqp_it == local_nqp.end())
      mooseError("No nqp found for element id ", elem_id);
    return {it->second, nqp_it->second, true};
  }

  if (_global_cache_ready)
  {
    if (auto it = _global_elem_batch.find(elem_id); it != _global_elem_batch.end())
      return {it->second.first, it->second.second, false};
  }

  mooseError("No batch index found for element id ", elem_id);
  return {};
}

NEML2ModelExecutor::BatchInfo
NEML2ModelExecutor::getSideBatchInfo(dof_id_type elem_id,
                                     unsigned int side,
                                     BoundaryID boundary_id) const
{
  if (!_side_batch_index_generator)
    mooseError("Side batch index generator is not configured.");

  const auto & local_map = _side_batch_index_generator->sideToBatchIndex();
  const auto & local_nqp = _side_batch_index_generator->sideToNQPs();
  NEML2SideBatchIndexGenerator::SideKey key{elem_id, side, boundary_id};

  if (auto it = local_map.find(key); it != local_map.end())
  {
    const auto nqp_it = local_nqp.find(key);
    if (nqp_it == local_nqp.end())
      mooseError("No nqp found for element id ", elem_id, " side ", side, " boundary ",
                 boundary_id);
    return {it->second + _side_batch_offset, nqp_it->second, true};
  }

  mooseError("No side batch index found for element id ", elem_id, " side ", side, " boundary ",
             boundary_id);
  return {};
}

const neml2::Tensor &
NEML2ModelExecutor::getOutputTensor(const neml2::VariableName & output_name, bool global) const
{
  if (!global)
    return _retrieved_outputs.at(output_name);
  const auto it = _global_outputs.find(output_name);
  if (it == _global_outputs.end())
    mooseError("No global output cache found for NEML2 output variable '", output_name, "'.");
  return it->second.tensor;
}

const neml2::Tensor &
NEML2ModelExecutor::getOutputDerivativeTensor(const neml2::VariableName & output_name,
                                              const neml2::VariableName & input_name,
                                              bool global) const
{
  if (!global)
    return _retrieved_derivatives.at(output_name).at(input_name);
  const auto it = _global_derivatives.find(output_name);
  if (it == _global_derivatives.end())
    mooseError("No global derivative cache found for NEML2 output variable '", output_name, "'.");
  const auto it2 = it->second.find(input_name);
  if (it2 == it->second.end())
    mooseError("No global derivative cache found for NEML2 derivative '",
               output_name,
               "' w.r.t. '",
               input_name,
               "'.");
  return it2->second.tensor;
}

const neml2::Tensor &
NEML2ModelExecutor::getOutputParameterDerivativeTensor(const neml2::VariableName & output_name,
                                                       const std::string & parameter_name,
                                                       bool global) const
{
  if (!global)
    return _retrieved_parameter_derivatives.at(output_name).at(parameter_name);
  const auto it = _global_parameter_derivatives.find(output_name);
  if (it == _global_parameter_derivatives.end())
    mooseError("No global parameter-derivative cache found for NEML2 output variable '",
               output_name,
               "'.");
  const auto it2 = it->second.find(parameter_name);
  if (it2 == it->second.end())
    mooseError("No global parameter-derivative cache found for NEML2 output variable '",
               output_name,
               "' w.r.t. parameter '",
               parameter_name,
               "'.");
  return it2->second.tensor;
}

void
NEML2ModelExecutor::addGatheredVariable(const UserObjectName & gatherer_name,
                                        const neml2::VariableName & var)
{
  if (_gathered_variable_names.count(var))
    paramError("gatherers",
               "The NEML2 input variable `",
               var,
               "` gathered by UO '",
               gatherer_name,
               "' is already gathered by another gatherer.");
  _gathered_variable_names.insert(var);
}

void
NEML2ModelExecutor::addGatheredParameter(const UserObjectName & gatherer_name,
                                         const std::string & param)
{
  if (_gathered_parameter_names.count(param))
    paramError("gatherers",
               "The NEML2 model parameter `",
               param,
               "` gathered by UO '",
               gatherer_name,
               "' is already gathered by another gatherer.");
  _gathered_parameter_names.insert(param);
}

void
NEML2ModelExecutor::initialize()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  _output_ready = false;
  _error = false;
  _error_message.clear();
  _side_batch_offset = 0;
  _side_batch_size = 0;
  _in_side.clear();
  _global_elem_batch.clear();
  _global_outputs.clear();
  _global_derivatives.clear();
  _global_parameter_derivatives.clear();
  _global_cache_ready = false;
}

void
NEML2ModelExecutor::meshChanged()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  _output_ready = false;
  _side_batch_offset = 0;
  _side_batch_size = 0;
  _in_side.clear();
  _global_elem_batch.clear();
  _global_outputs.clear();
  _global_derivatives.clear();
  _global_parameter_derivatives.clear();
  _global_cache_ready = false;
}

void
NEML2ModelExecutor::execute()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  const bool vol_empty = _batch_index_generator.isEmpty();
  const bool side_empty =
      !_side_batch_index_generator || _side_batch_index_generator->isEmpty();

  // If both batches are empty, we do not need to do anything
  if (vol_empty && side_empty)
    return;

  fillInputs();

  if (_t_step > 0)
  {
    applyPredictor();
    auto success = solve();
    if (success)
      extractOutputs();
  }
}

void
NEML2ModelExecutor::fillInputs()
{
  try
  {
    for (const auto & uo : _gatherers)
      uo->insertInto(_in, _model_params);
    for (const auto & uo : _side_gatherers)
      uo->insertInto(_in_side, _model_params);

    const bool has_side =
        _side_batch_index_generator && !_side_batch_index_generator->isEmpty();
    if (has_side)
    {
      _side_batch_offset = _batch_index_generator.getBatchIndex();
      _side_batch_size = _side_batch_index_generator->getBatchIndex();

      for (auto & [var, val] : _in_side)
      {
        if (_in.count(var))
          _in[var] = neml2::base_cat({ _in[var], val }, 0);
        else
          _in[var] = val;
      }
      _in_side.clear();
    }
    else
    {
      _side_batch_offset = 0;
      _side_batch_size = 0;
      _in_side.clear();
    }

    // Send input variables and parameters to device
    for (auto & [var, val] : _in)
      val = val.to(device());
    for (auto & [param, pval] : _model_params)
      pval = pval.to(device());

    // Update model parameters
    model().set_parameters(_model_params);
    _model_params.clear();

    // Request gradient for the model parameters that we request AD for
    for (const auto & [y, dy] : _retrieved_parameter_derivatives)
      for (const auto & [p, tensor] : dy)
        model().get_parameter(p).requires_grad_(true);
  }
  catch (std::exception & e)
  {
    mooseError("An error occurred while filling inputs for the NEML2 model. Error message:\n",
               e.what(),
               NEML2Utils::NEML2_help_message);
  }
}

void
NEML2ModelExecutor::applyPredictor()
{
  try
  {
    if (!_has_state || !_has_old_state)
      return;

    // Set trial state variables (i.e., initial guesses).
    // Right now we hard-code to use the old state as the trial state.
    // TODO: implement other predictors
    const auto & input_state = model().input_axis().subaxis(neml2::STATE);
    const auto & input_old_state = model().input_axis().subaxis(neml2::OLD_STATE);
    for (const auto & var : input_state.variable_names())
      if (input_old_state.has_variable(var))
        _in[var.prepend(neml2::STATE)] = _in[var.prepend(neml2::OLD_STATE)];
  }
  catch (std::exception & e)
  {
    mooseError("An error occurred while applying predictor for the NEML2 model. Error message:\n",
               e.what(),
               NEML2Utils::NEML2_help_message);
  }
}

void
NEML2ModelExecutor::expandInputs()
{
  // Figure out what our batch size is
  std::vector<neml2::Tensor> defined;
  for (const auto & [key, value] : _in)
    defined.push_back(value);
  const auto s = neml2::utils::broadcast_dynamic_sizes(defined);

  // Make all inputs conformal
  for (auto & [key, value] : _in)
    if (value.dynamic_sizes() != s)
      _in[key] = value.dynamic_unsqueeze(0).dynamic_expand(s);
}

bool
NEML2ModelExecutor::solve()
{
  try
  {
    // Evaluate the NEML2 material model
    TIME_SECTION("NEML2 solve", 3, "Solving NEML2 material model");

    // NEML2 requires double precision
    auto prev_dtype = neml2::get_default_dtype();
    neml2::set_default_dtype(neml2::kFloat64);

    if (scheduler())
    {
      // We only need consistent batch sizes if we are using the dispatcher
      expandInputs();
      neml2::ValueMapLoader loader(_in, 0);
      std::tie(_out, _dout_din) = dispatcher()->run(loader);
    }
    else
      std::tie(_out, _dout_din) = model().value_and_dvalue(_in);
    _in.clear();

    // Restore the default dtype
    neml2::set_default_dtype(prev_dtype);
  }
  catch (std::exception & e)
  {
    _error_message = e.what();
    _error = true;
  }

  return !_error;
}

void
NEML2ModelExecutor::extractOutputs()
{
  try
  {
    const auto N = _batch_index_generator.getBatchIndex() + _side_batch_size;

    // retrieve outputs
    for (auto & [y, target] : _retrieved_outputs)
      target = _out[y].to(output_device());

    // retrieve parameter derivatives
    for (auto & [y, dy] : _retrieved_parameter_derivatives)
      for (auto & [p, target] : dy)
        target = neml2::jacrev(_out[y],
                               model().get_parameter(p),
                               /*retain_graph=*/true,
                               /*create_graph=*/false,
                               /*allow_unused=*/false)
                     .to(output_device());

    // clear output
    _out.clear();

    // retrieve derivatives
    for (auto & [y, dy] : _retrieved_derivatives)
      for (auto & [x, target] : dy)
      {
        const auto & source = _dout_din[y][x];
        if (source.defined())
          target = source.to(output_device()).dynamic_expand({neml2::Size(N)});
      }

    // clear derivatives
    _dout_din.clear();
  }
  catch (std::exception & e)
  {
    mooseError("An error occurred while retrieving outputs from the NEML2 model. Error message:\n",
               e.what(),
               NEML2Utils::NEML2_help_message);
  }
}

void
NEML2ModelExecutor::syncGlobalOutputs()
{
  if (_communicator.size() <= 1)
  {
    _global_cache_ready = false;
    return;
  }

  const auto my_rank = _communicator.rank();
  const auto & mesh = _fe_problem.mesh().getMesh();

  // Build request list: ghost elements on this rank that are in the NEML2 blocks
  std::map<processor_id_type, std::vector<dof_id_type>> requests;
  for (auto it = mesh.semilocal_elements_begin(); it != mesh.semilocal_elements_end(); ++it)
  {
    const auto * elem = *it;
    if (!elem || !elem->active())
      continue;
    const auto owner = elem->processor_id();
    if (owner == my_rank)
      continue;
    if (!_batch_index_generator.hasBlocks(elem->subdomain_id()))
      continue;
    requests[owner].push_back(elem->id());
  }

  // Determine how many incoming request messages to expect
  std::vector<unsigned int> send_counts(_communicator.size(), 0);
  for (const auto & [pid, list] : requests)
    send_counts[pid] = static_cast<unsigned int>(list.size());

  std::vector<std::vector<unsigned int>> all_send_counts;
  _communicator.allgather(send_counts, all_send_counts, /* identical buffer lengths = */ true);

  unsigned int total_incoming_msgs = 0;
  for (const auto & per_rank : all_send_counts)
    if (per_rank[my_rank] > 0)
      total_incoming_msgs++;

  Parallel::MessageTag tag_requests = _communicator.get_unique_tag();
  Parallel::MessageTag tag_nqp = _communicator.get_unique_tag();

  std::size_t num_channels = 0;
  num_channels += _retrieved_outputs.size();
  for (const auto & [y, dy] : _retrieved_derivatives)
    num_channels += dy.size();
  for (const auto & [y, dy] : _retrieved_parameter_derivatives)
    num_channels += dy.size();

  std::vector<Parallel::MessageTag> data_tags;
  data_tags.reserve(num_channels);
  for (std::size_t i = 0; i < num_channels; ++i)
    data_tags.push_back(_communicator.get_unique_tag());

  // Send requests
  std::vector<Parallel::Request> send_reqs;
  for (const auto & [pid, list] : requests)
  {
    if (list.empty())
      continue;
    send_reqs.emplace_back();
    _communicator.send(pid, list, send_reqs.back(), tag_requests);
  }

  // Receive incoming requests
  std::map<processor_id_type, std::vector<dof_id_type>> incoming;
  for (unsigned int i = 0; i < total_incoming_msgs; ++i)
  {
    Parallel::Status status(_communicator.probe(Parallel::any_source, tag_requests));
    const auto source_pid = static_cast<processor_id_type>(status.source());
    std::vector<dof_id_type> req;
    _communicator.receive(source_pid, req, tag_requests);
    incoming[source_pid] = std::move(req);
  }

  const auto & local_map = _batch_index_generator.elemToBatchIndex();
  const auto & local_nqp = _batch_index_generator.elemToNQPs();

  // Respond to incoming requests with packed data
  std::vector<Parallel::Request> response_reqs;
  for (const auto & [pid, list] : incoming)
  {
    if (list.empty())
      continue;

    // Send nqp list
    std::vector<unsigned int> nqp_list;
    nqp_list.reserve(list.size());
    for (const auto & elem_id : list)
      nqp_list.push_back(local_nqp.at(elem_id));
    response_reqs.emplace_back();
    _communicator.send(pid, nqp_list, response_reqs.back(), tag_nqp);

    // Send outputs
    std::size_t tag_offset = 0;
    for (const auto & [y, tensor] : _retrieved_outputs)
    {
      auto data = packElementData(tensor, list, local_map, local_nqp);
      response_reqs.emplace_back();
      _communicator.send(pid, data, response_reqs.back(), data_tags[tag_offset++]);
    }

    // Send derivatives
    for (const auto & [y, dy] : _retrieved_derivatives)
      for (const auto & [x, tensor] : dy)
      {
        auto data = packElementData(tensor, list, local_map, local_nqp);
        response_reqs.emplace_back();
        _communicator.send(pid, data, response_reqs.back(), data_tags[tag_offset++]);
      }

    // Send parameter derivatives
    for (const auto & [y, dy] : _retrieved_parameter_derivatives)
      for (const auto & [p, tensor] : dy)
      {
        auto data = packElementData(tensor, list, local_map, local_nqp);
        response_reqs.emplace_back();
        _communicator.send(pid, data, response_reqs.back(), data_tags[tag_offset++]);
      }
  }

  // Receive responses for our own requests and build ghost cache
  _global_elem_batch.clear();
  _global_outputs.clear();
  _global_derivatives.clear();
  _global_parameter_derivatives.clear();

  std::size_t ghost_batch = 0;
  std::vector<dof_id_type> ghost_order;
  std::vector<unsigned int> ghost_nqp;

  for (const auto & [pid, list] : requests)
  {
    if (list.empty())
      continue;

    // Receive nqp list
    std::vector<unsigned int> nqp_list;
    _communicator.receive(pid, nqp_list, tag_nqp);
    if (nqp_list.size() != list.size())
      mooseError("Received nqp list size mismatch for NEML2 ghost sync.");

    for (std::size_t i = 0; i < list.size(); ++i)
    {
      _global_elem_batch[list[i]] = {ghost_batch, nqp_list[i]};
      ghost_order.push_back(list[i]);
      ghost_nqp.push_back(nqp_list[i]);
      ghost_batch += nqp_list[i];
    }

    std::size_t tag_offset = 0;
    // Receive outputs
    for (const auto & [y, tensor] : _retrieved_outputs)
    {
      std::vector<Real> data;
      _communicator.receive(pid, data, data_tags[tag_offset++]);
      auto & cache = _global_outputs[y];
      cache.data.insert(cache.data.end(), data.begin(), data.end());
    }

    // Receive derivatives
    for (const auto & [y, dy] : _retrieved_derivatives)
      for (const auto & [x, tensor] : dy)
      {
        std::vector<Real> data;
        _communicator.receive(pid, data, data_tags[tag_offset++]);
        auto & cache = _global_derivatives[y][x];
        cache.data.insert(cache.data.end(), data.begin(), data.end());
      }

    // Receive parameter derivatives
    for (const auto & [y, dy] : _retrieved_parameter_derivatives)
      for (const auto & [p, tensor] : dy)
      {
        std::vector<Real> data;
        _communicator.receive(pid, data, data_tags[tag_offset++]);
        auto & cache = _global_parameter_derivatives[y][p];
        cache.data.insert(cache.data.end(), data.begin(), data.end());
      }
  }

  // Build tensors from packed ghost data
  for (const auto & [y, tensor] : _retrieved_outputs)
  {
    auto & cache = _global_outputs[y];
    if (cache.data.empty())
      continue;
    std::vector<int64_t> shape(tensor.sizes().begin(), tensor.sizes().end());
    if (!shape.empty())
      shape[0] = static_cast<int64_t>(ghost_batch);
    auto t = at::from_blob(cache.data.data(),
                           shape,
                           at::TensorOptions().dtype(neml2::kFloat64));
    cache.tensor = neml2::Tensor(t, 1);
  }

  for (const auto & [y, dy] : _retrieved_derivatives)
    for (const auto & [x, tensor] : dy)
    {
      auto & cache = _global_derivatives[y][x];
      if (cache.data.empty())
        continue;
      std::vector<int64_t> shape(tensor.sizes().begin(), tensor.sizes().end());
      if (!shape.empty())
        shape[0] = static_cast<int64_t>(ghost_batch);
      auto t = at::from_blob(cache.data.data(),
                             shape,
                             at::TensorOptions().dtype(neml2::kFloat64));
      cache.tensor = neml2::Tensor(t, 1);
    }

  for (const auto & [y, dy] : _retrieved_parameter_derivatives)
    for (const auto & [p, tensor] : dy)
    {
      auto & cache = _global_parameter_derivatives[y][p];
      if (cache.data.empty())
        continue;
      std::vector<int64_t> shape(tensor.sizes().begin(), tensor.sizes().end());
      if (!shape.empty())
        shape[0] = static_cast<int64_t>(ghost_batch);
      auto t = at::from_blob(cache.data.data(),
                             shape,
                             at::TensorOptions().dtype(neml2::kFloat64));
      cache.tensor = neml2::Tensor(t, 1);
    }

  Parallel::wait(send_reqs);
  Parallel::wait(response_reqs);

  _global_cache_ready = ghost_batch > 0;
}

void
NEML2ModelExecutor::finalize()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  // See if any rank failed
  processor_id_type pid;
  _communicator.maxloc(_error, pid);

  // Fail the next nonlinear convergence check if any rank failed
  if (_error)
  {
    _communicator.broadcast(_error_message, pid);
    if (_communicator.rank() == 0)
    {
      std::string msg = "NEML2 model execution failed on at least one processor with ID " +
                        std::to_string(pid) + ". Error message:\n";
      msg += _error_message;
      if (_fe_problem.isTransient())
        msg += "\nTo recover, the solution will fail and then be re-attempted with a reduced time "
               "step.";
      _console << COLOR_YELLOW << msg << COLOR_DEFAULT << std::endl;
    }
    _fe_problem.setFailNextNonlinearConvergenceCheck();
  }
  else if (_t_step > 0)
  {
    syncGlobalOutputs();
    _output_ready = true;
  }
}

void
NEML2ModelExecutor::checkExecutionStage() const
{
  if (_fe_problem.startedInitialSetup())
    mooseError("NEML2 output variables and derivatives must be retrieved during object "
               "construction. This is a code problem.");
}

const neml2::Tensor &
NEML2ModelExecutor::getOutput(const neml2::VariableName & output_name) const
{
  checkExecutionStage();

  if (!model().output_axis().has_variable(output_name))
    mooseError("Trying to retrieve a non-existent NEML2 output variable '", output_name, "'.");

  return _retrieved_outputs[output_name];
}

const neml2::Tensor &
NEML2ModelExecutor::getOutputDerivative(const neml2::VariableName & output_name,
                                        const neml2::VariableName & input_name) const
{
  checkExecutionStage();

  if (!model().output_axis().has_variable(output_name))
    mooseError("Trying to retrieve the derivative of NEML2 output variable '",
               output_name,
               "' with respect to NEML2 input variable '",
               input_name,
               "', but the NEML2 output variable does not exist.");

  if (!model().input_axis().has_variable(input_name))
    mooseError("Trying to retrieve the derivative of NEML2 output variable '",
               output_name,
               "' with respect to NEML2 input variable '",
               input_name,
               "', but the NEML2 input variable does not exist.");

  return _retrieved_derivatives[output_name][input_name];
}

const neml2::Tensor &
NEML2ModelExecutor::getOutputParameterDerivative(const neml2::VariableName & output_name,
                                                 const std::string & parameter_name) const
{
  checkExecutionStage();

  if (!model().output_axis().has_variable(output_name))
    mooseError("Trying to retrieve the derivative of NEML2 output variable '",
               output_name,
               "' with respect to NEML2 model parameter '",
               parameter_name,
               "', but the NEML2 output variable does not exist.");

  if (model().named_parameters().count(parameter_name) != 1)
    mooseError("Trying to retrieve the derivative of NEML2 output variable '",
               output_name,
               "' with respect to NEML2 model parameter '",
               parameter_name,
               "', but the NEML2 model parameter does not exist.");

  return _retrieved_parameter_derivatives[output_name][parameter_name];
}

#endif
