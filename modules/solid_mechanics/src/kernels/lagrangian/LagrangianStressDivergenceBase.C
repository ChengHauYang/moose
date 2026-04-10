//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "LagrangianStressDivergenceBase.h"

InputParameters
LagrangianStressDivergenceBase::validParams()
{
  InputParameters params = KernelScalarBase::validParams();

  params.addRequiredParam<unsigned int>("component", "Which direction this kernel acts in");
  params.addRequiredCoupledVar("displacements", "The displacement components");

  params.addParam<bool>("large_kinematics", false, "Use large displacement kinematics");
  params.addParam<bool>("stabilize_strain", false, "Average the volumetric strains");

  params.addParam<std::string>("base_name", "Material property base name");

  params.addCoupledVar("temperature",
                       "The name of the temperature variable used in the "
                       "ComputeThermalExpansionEigenstrain.  (Not required for "
                       "simulations without temperature coupling.)");

  params.addParam<std::vector<MaterialPropertyName>>(
      "eigenstrain_names",
      {},
      "List of eigenstrains used in the strain calculation. Used for computing their derivatives "
      "for off-diagonal Jacobian terms.");

  params.addCoupledVar("out_of_plane_strain",
                       "The out-of-plane strain variable for weak plane stress formulation.");

  params.addCoupledVar(
      "additional_coupled_vars",
      "List of additional coupled variables which PK1 stress depends on. Derivatives of PK1 stress "
      "w.r.t each of the additional coupled variable should be provided as material properties "
      "through parameters `additional_coupling_jacobians` in the material block.");

  params.addParam<std::vector<MaterialPropertyName>>(
      "additional_coupling_jacobians",
      {},
      "Material properties storing derivative of PK1 (RankTwoTensor) w.r.t each additional var");

  return params;
}

LagrangianStressDivergenceBase::LagrangianStressDivergenceBase(const InputParameters & parameters)
  : JvarMapKernelInterface<DerivativeMaterialInterface<KernelScalarBase>>(parameters),
    _large_kinematics(getParam<bool>("large_kinematics")),
    _stabilize_strain(getParam<bool>("stabilize_strain")),
    _base_name(isParamValid("base_name") ? getParam<std::string>("base_name") + "_" : ""),
    _alpha(getParam<unsigned int>("component")),
    _ndisp(coupledComponents("displacements")),
    _disp_nums(_ndisp),
    _avg_grad_trial(_ndisp),
    _F_ust(
        getMaterialPropertyByName<RankTwoTensor>(_base_name + "unstabilized_deformation_gradient")),
    _F_avg(getMaterialPropertyByName<RankTwoTensor>(_base_name + "average_deformation_gradient")),
    _f_inv(getMaterialPropertyByName<RankTwoTensor>(_base_name +
                                                    "inverse_incremental_deformation_gradient")),
    _F_inv(getMaterialPropertyByName<RankTwoTensor>(_base_name + "inverse_deformation_gradient")),
    _F(getMaterialPropertyByName<RankTwoTensor>(_base_name + "deformation_gradient")),
    _temperature(isCoupled("temperature") ? getVar("temperature", 0) : nullptr),
    _out_of_plane_strain(isCoupled("out_of_plane_strain") ? getVar("out_of_plane_strain", 0)
                                                          : nullptr)
{
  // Do the vector coupling of the displacements
  for (unsigned int i = 0; i < _ndisp; i++)
    _disp_nums[i] = coupled("displacements", i);

  // We need to use identical discretizations for all displacement components
  auto order_x = getVar("displacements", 0)->order();
  for (unsigned int i = 1; i < _ndisp; i++)
  {
    if (getVar("displacements", i)->order() != order_x)
      mooseError("The Lagrangian StressDivergence kernels require equal "
                 "order interpolation for all displacements.");
  }

  // fetch eigenstrain derivatives
  const auto nvar = _coupled_moose_vars.size();
  _deigenstrain_dargs.resize(nvar);
  for (std::size_t i = 0; i < nvar; ++i)
    for (auto eigenstrain_name : getParam<std::vector<MaterialPropertyName>>("eigenstrain_names"))
      _deigenstrain_dargs[i].push_back(&getMaterialPropertyDerivative<RankTwoTensor>(
          eigenstrain_name, _coupled_moose_vars[i]->name()));

  _n_additional = coupledComponents("additional_coupled_vars");
  _additional_var_nums.resize(_n_additional);
  _additional_coupled_vars.resize(_n_additional);

  const auto & jac_names =
      getParam<std::vector<MaterialPropertyName>>("additional_coupling_jacobians");

  if (jac_names.size() != _n_additional)
    mooseError("Size mismatch: the length of 'additional_coupling_jacobians' (" +
               std::to_string(jac_names.size()) +
               ") must have the same length as "
               "'additional_coupled_vars' (" +
               std::to_string(_n_additional) + ").");

  _dpk1_dadditional.resize(_n_additional);

  for (unsigned int a = 0; a < _n_additional; ++a)
  {
    _additional_var_nums[a] = coupled("additional_coupled_vars", a);

    _additional_coupled_vars[a] = getVar("additional_coupled_vars", a);

    // Each jac property is a RankTwoTensor: dPK1 / d(var_a)
    _dpk1_dadditional[a] = &getMaterialPropertyByName<RankTwoTensor>(jac_names[a]);
  }
}

void
LagrangianStressDivergenceBase::precalculateJacobian()
{
  // Skip if we are not doing stabilization
  if (!_stabilize_strain)
    return;

  // We need the gradients of shape functions in the reference frame
  _fe_problem.prepareShapes(_var.number(), _tid);
  _avg_grad_trial[_alpha].resize(_phi.size());
  precalculateJacobianDisplacement(_alpha);
}

void
LagrangianStressDivergenceBase::precalculateOffDiagJacobian(unsigned int jvar)
{
  // Skip if we are not doing stabilization
  if (!_stabilize_strain)
    return;

  for (auto beta : make_range(_ndisp))
    if (jvar == _disp_nums[beta])
    {
      // We need the gradients of shape functions in the reference frame
      _fe_problem.prepareShapes(jvar, _tid);
      _avg_grad_trial[beta].resize(_phi.size());
      precalculateJacobianDisplacement(beta);
    }
}

Real
LagrangianStressDivergenceBase::computeQpJacobian()
{
  return computeQpJacobianDisplacement(_alpha, _alpha);
}

Real
LagrangianStressDivergenceBase::computeQpOffDiagJacobian(unsigned int jvar)
{
  // Bail if jvar not coupled
  if (getJvarMap()[jvar] < 0)
    return 0.0;

  // Off diagonal terms for other displacements
  for (auto beta : make_range(_ndisp))
    if (jvar == _disp_nums[beta])
      return computeQpJacobianDisplacement(_alpha, beta);

  // Off diagonal temperature term due to eigenstrain
  if (_temperature && jvar == _temperature->number())
    return computeQpJacobianTemperature(mapJvarToCvar(jvar));

  // Off diagonal term due to weak plane stress
  if (_out_of_plane_strain && jvar == _out_of_plane_strain->number())
    return computeQpJacobianOutOfPlaneStrain();

  // Off diagonal terms for additional coupled vars
  for (unsigned int a = 0; a < _n_additional; ++a)
    if (jvar == _additional_var_nums[a])
      return computeQpJacobianAdditionalCoupledVar(a);

  return 0;
}
