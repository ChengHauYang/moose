//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NEML2ModelExecutor.h"
#include "ElementSubdomainModifierBase.h"
#include "MOOSEToNEML2.h"
#include <set>

#ifdef NEML2_ENABLED
#include "neml2/tensors/functions/jacrev.h"
#include "neml2/dispatchers/ValueMapLoader.h"
#endif

registerMooseObject("SolidMechanicsApp", NEML2ModelExecutor);

InputParameters
NEML2ModelExecutor::actionParams()
{
  auto params = emptyInputParameters();
  // allow user to explicit skip required input variables
  params.addParam<std::vector<std::string>>(
      "skip_inputs",
      {},
      NEML2Utils::docstring(
          "List of NEML2 variables to skip error checking when setting up the model input. If an "
          "input variable is skipped, its value will stay zero. If a required input variable is "
          "not skipped, an error will be raised."));
  params.addParam<UserObjectName>("esm", "The ElementSubdomainModifierBase user object");
  return params;
}

InputParameters
NEML2ModelExecutor::validParams()
{
  auto params = NEML2ModelInterface<GeneralUserObject>::validParams();
  params += NEML2ModelExecutor::actionParams();
  params.addClassDescription(NEML2Utils::docstring("Execute the specified NEML2 model"));

  params.addRequiredParam<UserObjectName>(
      "batch_index_generator",
      "The NEML2BatchIndexGenerator used to generate the element-to-batch-index map.");

  params.addParam<std::vector<UserObjectName>>(
      "gatherers",
      {},
      NEML2Utils::docstring(
          "List of MOOSE*ToNEML2 user objects gathering MOOSE data as NEML2 input variables"));
  params.addParam<std::vector<UserObjectName>>(
      "param_gatherers",
      {},
      NEML2Utils::docstring(
          "List of MOOSE*ToNEML2 user objects gathering MOOSE data as NEML2 model parameters"));

  params.addParam<bool>("esm_required", false, "whether esm is there");

  params.addParam<NEML2Utils::CopyValueToOld>(
      "apply_new2old",
      NEML2Utils::CopyValueToOld::None,
      "how we apply strain to old strain for newly activated elements");

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
    _esm_required(getParam<bool>("esm_required")),
    _apply_new2old(getParam<NEML2Utils::CopyValueToOld>("apply_new2old")),
    _esm(_esm_required ? &getUserObject<ElementSubdomainModifierBase>("esm") : nullptr),
    /*_esm(&getUserObject<ElementSubdomainModifierBase>("esm")),*/
    /*_esm(getUserObject<ElementSubdomainModifierBase>("esm")),*/
    _output_ready(false)
#endif
{
#ifdef NEML2_ENABLED
  validateModel();

  // add user object dependencies by name (the UOs do not need to exist yet for this)
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("gatherers"))
    _depend_uo.insert(gatherer_name);
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("param_gatherers"))
    _depend_uo.insert(gatherer_name);

  // variables to skip error checking (converting vector to set to prevent duplicate checks)
  for (const auto & var_name : getParam<std::vector<std::string>>("skip_inputs"))
    _skip_vars.insert(NEML2Utils::parseVariableName(var_name));
#endif
}

#ifdef NEML2_ENABLED
void
NEML2ModelExecutor::initialSetup()
{

  // output_axis(): state/S| state/internal/ep
  // input_axis(): state/S| state/internal/ep

  // deal with user object provided inputs
  for (const auto & gatherer_name : getParam<std::vector<UserObjectName>>("gatherers"))
  {
#ifndef NDEBUG
    std::cout << "gatherer_name = " << gatherer_name << std::endl;
//     gatherer_name = __moose(neml2_strain)->neml2(forces/E)_A__
// gatherer_name = __moose(neml2_strain)->neml2(old_forces/E)_A__
// gatherer_name = __moose(time)->neml2(forces/t)A__
// gatherer_name = __moose(time)->neml2(old_forces/t)A__
// gatherer_name = __moose(neml2_stress)->neml2(old_state/S)_A__
// gatherer_name = __moose(equivalent_plastic_strain)->neml2(old_state/internal/ep)_A__
#endif
    // gather coupled user objects late to ensure they are constructed. Do not add them as
    // dependencies (that's already done in the constructor).
    const auto & uo = getUserObjectByName<MOOSEToNEML2>(gatherer_name, /*is_dependency=*/false);

#ifndef NDEBUG
    std::cout << "NEML2Utils::parseVariableName(uo.NEML2Name()) = "
              << NEML2Utils::parseVariableName(uo.NEML2Name()) << "\n";
#endif
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
}

std::size_t
NEML2ModelExecutor::getBatchIndex(dof_id_type elem_id) const
{
  return _batch_index_generator.getBatchIndex(elem_id);
}

int
NEML2ModelExecutor::getGPs() const
{
  return _batch_index_generator.getGPs();
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
NEML2ModelExecutor::meshChanged()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  _output_ready = false;
}

void
NEML2ModelExecutor::initialize()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  _output_ready = false;
}

void
NEML2ModelExecutor::execute()
{
  if (!NEML2Utils::shouldCompute(_fe_problem))
    return;

  try
  {
    fillInputs();

    if (_t_step > 0)
    {
      applyPredictor();
      solve();
      extractOutputs();

      _output_ready = true;
    }
  }
  catch (neml2::NEMLException & e)
  {
    mooseException("An error occurred during the evaluation of a NEML2 model:\n",
                   e.what(),
                   NEML2Utils::NEML2_help_message);
  }
  catch (std::runtime_error & e)
  {
    mooseException("An unknown error occurred during the evaluation of a NEML2 model:\n",
                   e.what(),
                   "\nIt is possible that this error is related to NEML2.",
                   NEML2Utils::NEML2_help_message);
  }
}

void
NEML2ModelExecutor::fillInputs()
{
  for (const auto & uo : _gatherers)
    uo->insertInto(_in, _model_params);

  // _in[neml2::VariableName("forces/E")];

  // Send input variables and parameters to device
  for (auto & [var, val] : _in)
  {
    // std::cout << "var = " << var << std::endl;
    val = val.to(device());
  }
  for (auto & [param, pval] : _model_params)
    pval = pval.to(device());

  // Update model parameters
  model().set_parameters(_model_params);
  _model_params.clear();

  // Request gradient for the model parameters that we request AD for
  // output variable y -> parameter p -> output Tensor
  for (const auto & [y, dy] : _retrieved_parameter_derivatives)
    for (const auto & [p, tensor] : dy)
      model().get_parameter(p).requires_grad_(true);

  if (_apply_new2old == NEML2Utils::CopyValueToOld::NewToOld)
  {
    _console << "Applying new to old" << std::endl;
    applyNewToOld(neml2::VariableName("forces", "E"));
  }
  else if (_apply_new2old == NEML2Utils::CopyValueToOld::TopToOld)
  {
    _console << "Applying top neighbor to old" << std::endl;
    applyTopNeighborToOld(neml2::VariableName("forces", "E"));
  }
  // else if (_apply_new2old == NEML2Utils::CopyValueToOld::None)
  // {
  //   _console << "Applying none" << std::endl;
  // }
  // else
  // {
  //   mooseError("Unknown value for apply_new2old: ", _apply_new2old);
  // }
}

void
NEML2ModelExecutor::applyPredictor()
{
  if (!model().input_axis().has_state())
    return;
  if (!model().input_axis().has_old_state())
    return;

  // Set trial state variables (i.e., initial guesses).
  // Right now we hard-code to use the old state as the trial state.
  // TODO: implement other predictors
  const auto input_state = model().input_axis().subaxis(neml2::STATE);
  const auto input_old_state = model().input_axis().subaxis(neml2::OLD_STATE);
  for (const auto & var : input_state.variable_names())
  {

#ifndef NDEBUG
    std::cout << "var = " << var << std::endl;
// var = S
// var = internal/ep
#endif

    if (input_old_state.has_variable(var))
      _in[var.prepend(neml2::STATE)] = _in[var.prepend(neml2::OLD_STATE)];
  }

#ifndef NDEBUG
  for (const auto & var_old : input_old_state.variable_names())
  {
    std::cout << "var_old = " << var_old << std::endl;
  }
#endif
}

void
NEML2ModelExecutor::expandInputs()
{
  // Figure out what our batch size is
  std::vector<neml2::Tensor> defined;
  for (const auto & [key, value] : _in)
    defined.push_back(value);
  const auto batch_shape = neml2::utils::broadcast_batch_sizes(defined);

  // Make all inputs conformal
  for (auto & [key, value] : _in)
    if (value.batch_sizes() != batch_shape)
      _in[key] = value.batch_unsqueeze(0).batch_expand(batch_shape);
}

void
NEML2ModelExecutor::expandInputs()
{
  // Figure out what our batch size is
  std::vector<neml2::Tensor> defined;
  for (const auto & [key, value] : _in)
    defined.push_back(value);
  const auto batch_shape = neml2::utils::broadcast_batch_sizes(defined);

  // Make all inputs conformal
  for (auto & [key, value] : _in)
    if (value.batch_sizes() != batch_shape)
      _in[key] = value.batch_unsqueeze(0).batch_expand(batch_shape);
}

void
NEML2ModelExecutor::expandInputs()
{
  // Figure out what our batch size is
  std::vector<neml2::Tensor> defined;
  for (const auto & [key, value] : _in)
    defined.push_back(value);
  const auto batch_shape = neml2::utils::broadcast_batch_sizes(defined);

  // Make all inputs conformal
  for (auto & [key, value] : _in)
    if (value.batch_sizes() != batch_shape)
      _in[key] = value.batch_unsqueeze(0).batch_expand(batch_shape);
}

void
NEML2ModelExecutor::solve()
{
  // Evaluate the NEML2 material model
  TIME_SECTION("NEML2 solve", 3, "Solving NEML2 material model");

  // NEML2 requires double precision
  auto prev_dtype = neml2::get_default_dtype();
  neml2::set_default_dtype(neml2::kFloat64);

  if (scheduler())
  {
    // We only need consisent batch sizes if we are using the dispatcher
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

void
NEML2ModelExecutor::extractOutputs()
{
  const auto N = _batch_index_generator.getBatchIndex();

  // retrieve outputs
  for (auto & [y, target] : _retrieved_outputs)
  {
    // std::cout << "y = " << y << std::endl;
    target = _out[y].to(torch::kCPU);
  }

  // retrieve parameter derivatives
  for (auto & [y, dy] : _retrieved_parameter_derivatives)
    for (auto & [p, target] : dy)
      target = neml2::jacrev(_out[y],
                             model().get_parameter(p),
                             /*retain_graph=*/true,
                             /*create_graph=*/false,
                             /*allow_unused=*/false)
                   .to(torch::kCPU);

  // clear output
  _out.clear();

  // retrieve derivatives
  for (auto & [y, dy] : _retrieved_derivatives)
    for (auto & [x, target] : dy)
    {
      const auto & source = _dout_din[y][x];
      if (source.defined())
      {

        // #ifndef NDEBUG
        // if (_esm == {})
        // {
        //   std::cout << "ElementSubdomainModifierBase user object is not provided." <<
        //   std::endl;
        // }

        // std::cout << "[DEBUG] Tensor derivative source for output '" << y << "', input '" << x
        //           << "':\n";
        // std::cout << "         source.sizes() = " << source.sizes() << "\n";
        // std::cout << "         expected batch size N = " << N << "\n";
        // #endif

        target = source.to(torch::kCPU).batch_expand({neml2::Size(N)});
      }
    }

  // clear derivatives
  _dout_din.clear();
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
  // std::cout << "output_name = " << output_name << "\n";
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

const int
NEML2ModelExecutor::findTopNeighbor(const Elem * elem)
{
  const auto & current_centroid = elem->vertex_average();
  const Real y_tolerance = 1e-8;

  const Elem * top_neighbor = nullptr;

  for (unsigned int side = 0; side < elem->n_sides(); ++side)
  {
    const Elem * neighbor = elem->neighbor_ptr(side);
    if (neighbor)
    {
      const auto & neighbor_centroid = neighbor->vertex_average();

      // Check if neighbor is vertically above current element
      if ((neighbor_centroid(1) - current_centroid(1)) > y_tolerance)
      {
        top_neighbor = neighbor;
        break; // Assuming only one top neighbor is needed
      }
    }
  }

  if (top_neighbor == nullptr)
  {
    mooseError("Top neighbor not found for element ", elem->id());
  }

  return top_neighbor->id();
}

bool
NEML2ModelExecutor::checkElemChanged(const Elem * elem)
{
  const auto elem_id = elem->id();
  const auto sub_id = elem->subdomain_id();

  bool ele_changed = false;

  auto it2 = _elem_to_subdomain_map.find(elem_id);
  if (it2 != _elem_to_subdomain_map.end())
  {
    if (it2->second != sub_id)
    {
      it2->second = sub_id;
      ele_changed = true;
    }
  }
  else
  {
    _elem_to_subdomain_map[elem_id] = sub_id;
    if (_t > _dt)
      ele_changed = true;
  }

  if (ele_changed)
  {
    std::cout << "Subdomain ID changed for element\n";
  }

  return ele_changed;
}

void
NEML2ModelExecutor::applyNewToOld(const neml2::VariableName & current_name)
{
  const auto old_name = current_name.old();

  auto & current_tensor = _in[current_name];
  auto & old_tensor = _in[old_name];

  bool print_debug = false;
  std::vector<neml2::Size> idx;
  for (auto it = _fe_problem.mesh().activeLocalElementsBegin();
       it != _fe_problem.mesh().activeLocalElementsEnd();
       ++it)
  {
    if (checkElemChanged(*it))
    {
      // std::cout << "element change\n";
      // Copy current -> old for this element
      const auto batch_index = _batch_index_generator.getBatchIndex((*it)->id());
      const auto n_qp = getGPs(); // Number of quadrature points per element
      print_debug = true;

      for (int qp = 0; qp < n_qp; ++qp)
      {
        const auto i = batch_index + qp;
        idx.push_back(neml2::Size(i));
      }
    }
  }

  auto idxt = torch::from_blob(idx.data(), {static_cast<long long>(idx.size())}, torch::kInt64);

#ifndef NDEBUG
  // if (print_debug)
  // {
  // for (auto i : idx)
  // {
  //   std::cout << "i = " << i << std::endl;
  // }
  // std::cout << "idxt = " << idxt << std::endl;
  // }
#endif

  auto cur_var = current_tensor.index({idxt});

  if (print_debug)
  {
    std::cout << "----- (before) ------\n";
    printAndCompareTensors(current_tensor, old_tensor, idxt);
  }

  old_tensor.index_put_({idxt}, cur_var);

  // #ifndef NDEBUG
  if (print_debug)
  {
    std::cout << "----- (after) ------\n";
    printAndCompareTensors(current_tensor, old_tensor, idxt);
  }

  // #endif
}

void
NEML2ModelExecutor::applyTopNeighborToOld(const neml2::VariableName & current_name)
{
  const auto old_name = current_name.old();

  auto & current_tensor = _in[current_name];
  auto & old_tensor = _in[old_name];

  bool print_debug = false;
  std::vector<neml2::Size> idx, idx_top;
  for (auto it = _fe_problem.mesh().activeLocalElementsBegin();
       it != _fe_problem.mesh().activeLocalElementsEnd();
       ++it)
  {
    if (checkElemChanged(*it))
    {
      print_debug = true;
      // Copy current -> old for this element
      const auto batch_index = _batch_index_generator.getBatchIndex((*it)->id());
      const auto n_qp = getGPs(); // Number of quadrature points per element

      int top_neighbor_id = findTopNeighbor(*it);
      const auto batch_index_top_neighbor = _batch_index_generator.getBatchIndex(top_neighbor_id);

      for (int qp = 0; qp < n_qp; ++qp)
      {
        const auto i = batch_index + qp;
        idx.push_back(neml2::Size(i));
        idx_top.push_back(neml2::Size(batch_index_top_neighbor + qp));
      }
    }
  }

  auto idxt = torch::from_blob(idx.data(), {static_cast<long long>(idx.size())}, torch::kInt64);
  auto idxt_top =
      torch::from_blob(idx_top.data(), {static_cast<long long>(idx_top.size())}, torch::kInt64);

  auto cur_var_top = current_tensor.index({idxt_top});
  auto old_var_top = old_tensor.index({idxt_top});

  if (print_debug)
  {
    std::cout << "----- (before) ------\n";
    printAndCompareTensors(current_tensor, old_tensor, idxt);
  }

  // original implementation
  // current_tensor.index_put_({idxt}, cur_var_top);
  // old_tensor.index_put_({idxt}, old_var_top);

  // Bad: original new -> original old | top new -> original new
  // old_tensor.index_put_({idxt}, current_tensor.index({idxt}));
  // current_tensor.index_put_({idxt}, cur_var_top);

  // Try: top old -> original old
  old_tensor.index_put_({idxt}, cur_var_top);

  if (print_debug)
  {
    std::cout << "----- (after) ------\n";
    printAndCompareTensors(current_tensor, old_tensor, idxt);
  }
}

void
NEML2ModelExecutor::printAndCompareTensors(const torch::Tensor & cur_var,
                                           const torch::Tensor & old_var,
                                           const torch::Tensor & idxt)
{
  auto cur_cpu = cur_var.index({idxt}).to(torch::kCPU).detach();
  auto old_cpu = old_var.index({idxt}).to(torch::kCPU).detach();

  auto diff = cur_cpu - old_cpu;
  auto avg_cur = cur_cpu.mean();
  auto avg_old = old_cpu.mean();
  auto avg_diff = diff.mean();

  std::cout << "== Tensor Debug Info ==\n";
  // std::cout << "Selected indices: " << idxt << "\n";
  // std::cout << "Current values:\n" << cur_cpu << "\n";
  // std::cout << "Old values:\n" << old_cpu << "\n";
  // std::cout << "Difference (current - old):\n" << diff << "\n";
  std::cout << "Average current: " << avg_cur.item<double>() << "\n";
  std::cout << "Average old: " << avg_old.item<double>() << "\n";
  std::cout << "Average diff: " << avg_diff.item<double>() << "\n";
}

#endif
