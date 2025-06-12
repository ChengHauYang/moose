#include "SpatioTemporalHeatAction.h"
#include "Factory.h"
#include "FEProblem.h"
#include "MooseMesh.h"
#include "Material.h"

// Register for different phases of the simulation
registerMooseAction("MooseApp", SpatioTemporalHeatAction, "add_user_object");
registerMooseAction("MooseApp", SpatioTemporalHeatAction, "add_mesh_modifier");
registerMooseAction("MooseApp", SpatioTemporalHeatAction, "add_material");
registerMooseAction("MooseApp", SpatioTemporalHeatAction, "add_kernel");

InputParameters
SpatioTemporalHeatAction::validParams()
{
  InputParameters params = Action::validParams();
  params.addRequiredParam<FileName>("path_file", "CSV file for spatiotemporal path");

  // for CSVPiecewiseLinearSpatioTemporalPath
  params.addParam<bool>("verbose", false, "Verbose path output");

  // for SpatioTemporalPathElementSubdomainModifier
  params.addParam<std::vector<SubdomainName>>(
      "block", {}, "Subdomain blocks for distinguishing active/inactive elements");
  params.addRequiredParam<SubdomainName>(
      "target_subdomain", "The subdomain name/ID to set when the path goes through an element");
  params.addRequiredParam<Real>("radius",
                                "The element subdomain is changed to the target subdomain if its "
                                "centroid is within the radius of the current path front.");

  ExecFlagEnum & exec = params.set<ExecFlagEnum>("execute_on_esm");
  exec.addAvailableFlags(EXEC_INITIAL, EXEC_TIMESTEP_BEGIN, EXEC_TIMESTEP_END, EXEC_FINAL);
  params.setDocString("execute_on_esm", exec.getDocString());

  params.addParam<bool>("within_elem_test",
                        false,
                        "Switch between using the within element test or the centroid test.");

  // for ESM to set the initial condition
  params.addParam<std::vector<SubdomainName>>(
      "unsolved_blocks",
      {},
      "If the region has the unsolved blocks, you should set this parameter to turn on the "
      "extrapolation of the solution to the newly activated elements.");
  MooseEnum init_strategy("IC_DEFAULT IC_POLYNOMIAL "
                          "IC_POLYNOMIAL_WHOLE_SOLVED_DOMAIN IC_POLYNOMIAL_THRESHOLD",
                          "IC_DEFAULT");

  params.addParam<std::vector<MooseEnum>>(
      "ic_strategy",
      {init_strategy},
      "The strategy to set the initial condition on the newly activated nodes. ");

  params.addParam<std::vector<UserObjectName>>(
      "nodal_patch_recovery_uo",
      {},
      "List of NodalPatchRecovery UserObjects for each component (e.g., u, v)");

  params.addParam<int>(
      "kd_tree_leaf_max_size", 10, "Maximum number of elements in a leaf node of the k-d tree");

  params.addParam<int>("nearby_element_threshold",
                       1,
                       "Threshold for considering elements as 'nearby' in the k-d tree search.");

  params.addParam<double>("radius_search_threshold",
                          -1.0,
                          "Threshold for considering elements as 'nearby' in the k-d tree search.");

  // for ADMovingEllipsoidalHeatSource
  params.addRequiredParam<MaterialPropertyName>("power", "Input power of the heat source.");
  params.addRequiredParam<MaterialPropertyName>(
      "a", "Length of the ellipsoid semi-axis along the path direction");
  params.addRequiredParam<MaterialPropertyName>(
      "b", "Length of the ellipsoid semi-axis perpendicular to the path direction");
  params.addParam<Real>("efficiency", 1.0, "Process efficiency");
  params.addParam<Real>("scale", 1.0, "Scaling factor");

  // for ADMatHeatSource (kernel)
  params.addRequiredParam<NonlinearVariableName>("heat_variable", "Temperature variable name");

  return params;
}

SpatioTemporalHeatAction::SpatioTemporalHeatAction(const InputParameters & params) : Action(params)
{
}

void
SpatioTemporalHeatAction::act()
{
  const std::string path_name = _name + "path";

  if (_current_task == "add_user_object")
  {
    InputParameters path_params = _factory.getValidParams("CSVPiecewiseLinearSpatioTemporalPath");
    path_params.set<FileName>("file") = getParam<FileName>("path_file");
    path_params.set<bool>("verbose") = getParam<bool>("verbose");

    _problem->addUserObject("CSVPiecewiseLinearSpatioTemporalPath", path_name, path_params);
  }

  else if (_current_task == "add_mesh_modifier")
  {
    InputParameters esm_params =
        _factory.getValidParams("SpatioTemporalPathElementSubdomainModifier");
    esm_params.set<std::string>("path") = path_name;
    esm_params.set<Real>("radius") = getParam<Real>("radius");
    esm_params.set<SubdomainName>("target_subdomain") = getParam<SubdomainName>("target_subdomain");
    esm_params.set<bool>("within_elem_test") = getParam<bool>("within_elem_test");
    esm_params.set<std::vector<SubdomainName>>("block") =
        getParam<std::vector<SubdomainName>>("block");
    esm_params.set<ExecFlagEnum>("execute_on") = getParam<ExecFlagEnum>("execute_on_esm");
    esm_params.set<std::vector<SubdomainName>>("unsolved_blocks") =
        getParam<std::vector<SubdomainName>>("unsolved_blocks");
    esm_params.set<std::vector<MooseEnum>>("ic_strategy") =
        getParam<std::vector<MooseEnum>>("ic_strategy");
    esm_params.set<std::vector<UserObjectName>>("nodal_patch_recovery_uo") =
        getParam<std::vector<UserObjectName>>("nodal_patch_recovery_uo");
    esm_params.set<int>("kd_tree_leaf_max_size") = getParam<int>("kd_tree_leaf_max_size");
    esm_params.set<int>("nearby_element_threshold") = getParam<int>("nearby_element_threshold");
    esm_params.set<double>("radius_search_threshold") = getParam<double>("radius_search_threshold");

    _problem->addUserObject("SpatioTemporalPathElementSubdomainModifier", "esm", esm_params);
  }

  else if (_current_task == "add_material")
  {
    InputParameters mat_params = _factory.getValidParams("ADMovingEllipsoidalHeatSource");
    mat_params.set<std::string>("path") = path_name;
    mat_params.set<MaterialPropertyName>("power") = getParam<MaterialPropertyName>("power");
    mat_params.set<Real>("efficiency") = getParam<Real>("efficiency");
    mat_params.set<Real>("scale") = getParam<Real>("scale");
    mat_params.set<MaterialPropertyName>("a") = getParam<MaterialPropertyName>("a");
    mat_params.set<MaterialPropertyName>("b") = getParam<MaterialPropertyName>("b");
    // mat_params.set<std::vector<std::string>>("outputs") = {"exodus"};

    _problem->addMaterial("ADMovingEllipsoidalHeatSource", "volumetric_heat", mat_params);
  }

  else if (_current_task == "add_kernel")
  {
    InputParameters k_params = _factory.getValidParams("ADMatHeatSource");
    k_params.set<MaterialPropertyName>("material_property") = "volumetric_heat";
    k_params.set<NonlinearVariableName>("variable") =
        getParam<NonlinearVariableName>("heat_variable");

    _problem->addKernel("ADMatHeatSource", "hsource", k_params);
  }
}
