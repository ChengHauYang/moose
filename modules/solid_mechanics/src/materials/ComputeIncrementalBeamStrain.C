//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeIncrementalBeamStrain.h"
#include "MooseMesh.h"
#include "Assembly.h"
#include "NonlinearSystem.h"
#include "MooseVariable.h"
#include "Function.h"

#include "libmesh/quadrature.h"
#include "libmesh/utility.h"

registerMooseObject("SolidMechanicsApp", ComputeIncrementalBeamStrain);
registerMooseObject("SolidMechanicsApp", ADComputeIncrementalBeamStrain);

template <bool is_ad>
InputParameters
ComputeIncrementalBeamStrainTempl<is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription("Compute a infinitesimal/large strain increment for the beam.");
  params.addRequiredCoupledVar(
      "rotations", "The rotations appropriate for the simulation geometry and coordinate system");
  params.addRequiredCoupledVar(
      "displacements",
      "The displacements appropriate for the simulation geometry and coordinate system");
  params.addRequiredCoupledVar(
      "area",
      "Cross-section area of the beam. Can be supplied as either a number or a variable name.");
  params.addCoupledVar("Ay",
                       0.0,
                       "First moment of area of the beam about y axis. Can be supplied "
                       "as either a number or a variable name.");
  params.addCoupledVar("Az",
                       0.0,
                       "First moment of area of the beam about z axis. Can be supplied "
                       "as either a number or a variable name.");
  params.addCoupledVar("Ix",
                       "Second moment of area of the beam about x axis. Can be "
                       "supplied as either a number or a variable name. Defaults to Iy+Iz.");
  params.addRequiredCoupledVar("Iy",
                               "Second moment of area of the beam about y axis. Can be "
                               "supplied as either a number or a variable name.");
  params.addRequiredCoupledVar("Iz",
                               "Second moment of area of the beam about z axis. Can be "
                               "supplied as either a number or a variable name.");
  params.addRequiredParam<RealGradient>("y_orientation",
                                        "Orientation of the y direction along "
                                        "with Iyy is provided. This should be "
                                        "perpendicular to the axis of the beam.");
  params.addParam<bool>("large_strain", false, "Set to true if large strain are to be calculated.");
  params.addParam<std::vector<MaterialPropertyName>>(
      "eigenstrain_names",
      {},
      "List of beam eigenstrains to be applied in this strain calculation.");
  params.addParam<FunctionName>(
      "elasticity_prefactor",
      "Optional function to use as a scalar prefactor on the elasticity vector for the beam.");
  return params;
}

template <bool is_ad>
ComputeIncrementalBeamStrainTempl<is_ad>::ComputeIncrementalBeamStrainTempl(
    const InputParameters & parameters)
  : Material(parameters),
    _has_Ix(this->isParamValid("Ix")),
    _nrot(this->coupledComponents("rotations")),
    _ndisp(this->coupledComponents("displacements")),
    _rot_num(_nrot),
    _disp_num(_ndisp),
    _area(this->coupledValue("area")),
    _Ay(this->coupledValue("Ay")),
    _Az(this->coupledValue("Az")),
    _Iy(this->coupledValue("Iy")),
    _Iz(this->coupledValue("Iz")),
    _Ix(_has_Ix ? this->coupledValue("Ix")
                : _zero), // falls back to Iy+Iz in computeQpStrain/computeStiffnessMatrix when
                          // _has_Ix is false
    _original_local_config(
        this->template declareRestartableData<RankTwoTensor>("original_local_config")),
    _original_length(this->template declareProperty<Real>("original_length")),
    _total_rotation(this->template declareProperty<RankTwoTensor>("total_rotation")),
    _total_disp_strain(
        this->template declareGenericProperty<RealVectorValue, is_ad>("total_disp_strain")),
    _total_rot_strain(
        this->template declareGenericProperty<RealVectorValue, is_ad>("total_rot_strain")),
    _total_disp_strain_old(
        this->template getMaterialPropertyOld<RealVectorValue>("total_disp_strain")),
    _total_rot_strain_old(
        this->template getMaterialPropertyOld<RealVectorValue>("total_rot_strain")),
    _mech_disp_strain_increment(this->template declareGenericProperty<RealVectorValue, is_ad>(
        "mech_disp_strain_increment")),
    _mech_rot_strain_increment(
        this->template declareGenericProperty<RealVectorValue, is_ad>("mech_rot_strain_increment")),
    _material_stiffness(
        this->template getMaterialPropertyByName<RealVectorValue>("material_stiffness")),
    _K11(this->template declareProperty<RankTwoTensor>("Jacobian_11")),
    _K21_cross(this->template declareProperty<RankTwoTensor>("Jacobian_12")),
    _K21(this->template declareProperty<RankTwoTensor>("Jacobian_21")),
    _K22(this->template declareProperty<RankTwoTensor>("Jacobian_22")),
    _K22_cross(this->template declareProperty<RankTwoTensor>("Jacobian_22_cross")),
    _large_strain(this->template getParam<bool>("large_strain")),
    _eigenstrain_names(
        this->template getParam<std::vector<MaterialPropertyName>>("eigenstrain_names")),
    _disp_eigenstrain(_eigenstrain_names.size()),
    _rot_eigenstrain(_eigenstrain_names.size()),
    _disp_eigenstrain_old(_eigenstrain_names.size()),
    _rot_eigenstrain_old(_eigenstrain_names.size()),
    _nonlinear_sys(_fe_problem.getNonlinearSystemBase(/*nl_sys_num=*/0)),
    _soln_disp_index_0(_ndisp),
    _soln_disp_index_1(_ndisp),
    _soln_rot_index_0(_ndisp),
    _soln_rot_index_1(_ndisp),
    _initial_rotation(this->template declareProperty<RankTwoTensor>("initial_rotation")),
    _effective_stiffness(this->template declareProperty<Real>("effective_stiffness")),
    _prefactor_function(this->isParamValid("elasticity_prefactor")
                            ? &this->getFunction("elasticity_prefactor")
                            : nullptr)
{
  if (_ndisp != _nrot)
    this->mooseError(
        "ComputeIncrementalBeamStrain: The number of variables supplied in 'displacements' "
        "and 'rotations' must match.");

  for (unsigned int i = 0; i < _ndisp; ++i)
  {
    MooseVariable * disp_variable = this->getVar("displacements", i);
    _disp_num[i] = disp_variable->number();

    MooseVariable * rot_variable = this->getVar("rotations", i);
    _rot_num[i] = rot_variable->number();
  }

  // For the AD path, request nodal DOF value references here in the constructor so that
  // needSolutionState() is invoked before the threaded element loop begins.  Calling
  // this->coupledDofValuesOld() / this->adCoupledDofValues() inside computeProperties() triggers
  // needSolutionState() during threading, which MOOSE asserts is forbidden.
  if constexpr (is_ad)
  {
    _ad_disp_dof_values.resize(_ndisp);
    _ad_rot_dof_values.resize(_ndisp);
    _disp_dof_values_old.resize(_ndisp);
    _rot_dof_values_old.resize(_ndisp);
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      _ad_disp_dof_values[i] = &this->adCoupledDofValues("displacements", i);
      _ad_rot_dof_values[i] = &this->adCoupledDofValues("rotations", i);
      _disp_dof_values_old[i] = &this->coupledDofValuesOld("displacements", i);
      _rot_dof_values_old[i] = &this->coupledDofValuesOld("rotations", i);
    }
  }

  if (_large_strain && (_Ay[0] > 0.0 || _Ay[1] > 0.0 || _Az[0] > 0.0 || _Az[1] > 0.0))
    this->mooseError(
        "ComputeIncrementalBeamStrain: Large strain calculation does not currently "
        "support asymmetric beam configurations with non-zero first or third moments of "
        "area.");

  for (unsigned int i = 0; i < _eigenstrain_names.size(); ++i)
  {
    _disp_eigenstrain[i] = &this->template getGenericMaterialProperty<RealVectorValue, is_ad>(
        "disp_" + _eigenstrain_names[i]);
    _rot_eigenstrain[i] = &this->template getGenericMaterialProperty<RealVectorValue, is_ad>(
        "rot_" + _eigenstrain_names[i]);
    _disp_eigenstrain_old[i] =
        &this->template getMaterialPropertyOld<RealVectorValue>("disp_" + _eigenstrain_names[i]);
    _rot_eigenstrain_old[i] =
        &this->template getMaterialPropertyOld<RealVectorValue>("rot_" + _eigenstrain_names[i]);
  }
}

template <bool is_ad>
void
ComputeIncrementalBeamStrainTempl<is_ad>::initQpStatefulProperties()
{
  const std::vector<RealGradient> * orientation =
      &_subproblem.assembly(_tid, _nonlinear_sys.number())
           .getFE(FEType().set_p_refinement(false), 1)
           ->get_dxyzdxi();
  RealGradient x_orientation = (*orientation)[0];
  x_orientation /= x_orientation.norm();

  RealGradient y_orientation = this->template getParam<RealGradient>("y_orientation");
  y_orientation /= y_orientation.norm();
  Real sum = x_orientation(0) * y_orientation(0) + x_orientation(1) * y_orientation(1) +
             x_orientation(2) * y_orientation(2);

  if (std::abs(sum) > 1e-4)
    this->mooseError("ComputeIncrementalBeamStrain: y_orientation should be perpendicular to "
                     "the axis of the beam.");

  RealGradient z_orientation;
  z_orientation(0) = (x_orientation(1) * y_orientation(2) - x_orientation(2) * y_orientation(1));
  z_orientation(1) = (x_orientation(2) * y_orientation(0) - x_orientation(0) * y_orientation(2));
  z_orientation(2) = (x_orientation(0) * y_orientation(1) - x_orientation(1) * y_orientation(0));

  _original_local_config(0, 0) = x_orientation(0);
  _original_local_config(0, 1) = x_orientation(1);
  _original_local_config(0, 2) = x_orientation(2);
  _original_local_config(1, 0) = y_orientation(0);
  _original_local_config(1, 1) = y_orientation(1);
  _original_local_config(1, 2) = y_orientation(2);
  _original_local_config(2, 0) = z_orientation(0);
  _original_local_config(2, 1) = z_orientation(1);
  _original_local_config(2, 2) = z_orientation(2);

  _total_rotation[_qp] = _original_local_config;

  RealVectorValue temp;
  _total_disp_strain[_qp] = temp;
  _total_rot_strain[_qp] = temp;
}

template <bool is_ad>
void
ComputeIncrementalBeamStrainTempl<is_ad>::computeProperties()
{
  std::vector<const Node *> node;
  for (unsigned int i = 0; i < 2; ++i)
    node.push_back(_current_elem->node_ptr(i));

  RealGradient dxyz;
  for (unsigned int i = 0; i < _ndisp; ++i)
    dxyz(i) = (*node[1])(i) - (*node[0])(i);

  _original_length[0] = dxyz.norm();

  if constexpr (is_ad)
  {
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      // Use pre-registered references (stored in constructor) to avoid calling
      // this->coupledDofValuesOld() / this->adCoupledDofValues() during the threaded element loop.
      _disp0(i) = (*_ad_disp_dof_values[i])[0] - (*_disp_dof_values_old[i])[0];
      _disp1(i) = (*_ad_disp_dof_values[i])[1] - (*_disp_dof_values_old[i])[1];
      _rot0(i) = (*_ad_rot_dof_values[i])[0] - (*_rot_dof_values_old[i])[0];
      _rot1(i) = (*_ad_rot_dof_values[i])[1] - (*_rot_dof_values_old[i])[1];
    }
  }
  else
  {
    const NumericVector<Number> & sol = *_nonlinear_sys.currentSolution();
    const NumericVector<Number> & sol_old = _nonlinear_sys.solutionOld();

    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      _soln_disp_index_0[i] = node[0]->dof_number(_nonlinear_sys.number(), _disp_num[i], 0);
      _soln_disp_index_1[i] = node[1]->dof_number(_nonlinear_sys.number(), _disp_num[i], 0);
      _soln_rot_index_0[i] = node[0]->dof_number(_nonlinear_sys.number(), _rot_num[i], 0);
      _soln_rot_index_1[i] = node[1]->dof_number(_nonlinear_sys.number(), _rot_num[i], 0);

      _disp0(i) = sol(_soln_disp_index_0[i]) - sol_old(_soln_disp_index_0[i]);
      _disp1(i) = sol(_soln_disp_index_1[i]) - sol_old(_soln_disp_index_1[i]);
      _rot0(i) = sol(_soln_rot_index_0[i]) - sol_old(_soln_rot_index_0[i]);
      _rot1(i) = sol(_soln_rot_index_1[i]) - sol_old(_soln_rot_index_1[i]);
    }
  }

  computeRotation();
  _initial_rotation[0] = _original_local_config;

  for (_qp = 0; _qp < _qrule->n_points(); ++_qp)
    computeQpStrain();

  if (_fe_problem.currentlyComputingJacobian())
    if constexpr (!is_ad)
      computeStiffnessMatrix();
}

template <bool is_ad>
void
ComputeIncrementalBeamStrainTempl<is_ad>::computeQpStrain()
{
  const Real A_avg = (_area[0] + _area[1]) / 2.0;
  const Real Iz_avg = (_Iz[0] + _Iz[1]) / 2.0;
  Real Ix = _Ix[_qp];
  if (!_has_Ix)
    Ix = _Iy[_qp] + _Iz[_qp];

  const GenericRealVectorValue<is_ad> grad_disp_0(1.0 / _original_length[0] * (_disp1 - _disp0));
  const GenericRealVectorValue<is_ad> grad_rot_0(1.0 / _original_length[0] * (_rot1 - _rot0));
  const GenericRealVectorValue<is_ad> avg_rot(
      0.5 * (_rot0(0) + _rot1(0)), 0.5 * (_rot0(1) + _rot1(1)), 0.5 * (_rot0(2) + _rot1(2)));

  _grad_disp_0_local_t = _total_rotation[0] * grad_disp_0;
  _grad_rot_0_local_t = _total_rotation[0] * grad_rot_0;
  _avg_rot_local_t = _total_rotation[0] * avg_rot;

  _mech_disp_strain_increment[_qp](0) = _grad_disp_0_local_t(0) * _area[_qp] -
                                        _grad_rot_0_local_t(2) * _Ay[_qp] +
                                        _grad_rot_0_local_t(1) * _Az[_qp];
  _mech_disp_strain_increment[_qp](1) = -_avg_rot_local_t(2) * _area[_qp] +
                                        _grad_disp_0_local_t(1) * _area[_qp] -
                                        _grad_rot_0_local_t(0) * _Az[_qp];
  _mech_disp_strain_increment[_qp](2) = _avg_rot_local_t(1) * _area[_qp] +
                                        _grad_disp_0_local_t(2) * _area[_qp] +
                                        _grad_rot_0_local_t(0) * _Ay[_qp];

  const Real Iyz = 0;
  _mech_rot_strain_increment[_qp](0) =
      _avg_rot_local_t(1) * _Ay[_qp] + _grad_disp_0_local_t(2) * _Ay[_qp] +
      _grad_rot_0_local_t(0) * Ix + _avg_rot_local_t(2) * _Az[_qp] -
      _grad_disp_0_local_t(1) * _Az[_qp];
  _mech_rot_strain_increment[_qp](1) = _grad_disp_0_local_t(0) * _Az[_qp] -
                                       _grad_rot_0_local_t(2) * Iyz +
                                       _grad_rot_0_local_t(1) * _Iz[_qp];
  _mech_rot_strain_increment[_qp](2) = -_grad_disp_0_local_t(0) * _Ay[_qp] +
                                       _grad_rot_0_local_t(2) * _Iy[_qp] -
                                       _grad_rot_0_local_t(1) * Iyz;

  if (_large_strain)
  {
    _mech_disp_strain_increment[_qp](0) +=
        0.5 *
        ((Utility::pow<2>(_grad_disp_0_local_t(0)) + Utility::pow<2>(_grad_disp_0_local_t(1)) +
          Utility::pow<2>(_grad_disp_0_local_t(2))) *
             _area[_qp] +
         Utility::pow<2>(_grad_rot_0_local_t(2)) * _Iy[_qp] +
         Utility::pow<2>(_grad_rot_0_local_t(1)) * _Iz[_qp] +
         Utility::pow<2>(_grad_rot_0_local_t(0)) * Ix);
    _mech_disp_strain_increment[_qp](1) += (-_avg_rot_local_t(2) * _grad_disp_0_local_t(0) +
                                            _avg_rot_local_t(0) * _grad_disp_0_local_t(2)) *
                                           _area[_qp];
    _mech_disp_strain_increment[_qp](2) += (_avg_rot_local_t(1) * _grad_disp_0_local_t(0) -
                                            _avg_rot_local_t(0) * _grad_disp_0_local_t(1)) *
                                           _area[_qp];

    _mech_rot_strain_increment[_qp](0) += -_avg_rot_local_t(1) * _grad_rot_0_local_t(2) * _Iy[_qp] +
                                          _avg_rot_local_t(2) * _grad_rot_0_local_t(1) * _Iz[_qp];
    _mech_rot_strain_increment[_qp](1) += (_grad_disp_0_local_t(0) * _grad_rot_0_local_t(1) -
                                           _grad_disp_0_local_t(1) * _grad_rot_0_local_t(0)) *
                                          _Iz[_qp];
    _mech_rot_strain_increment[_qp](2) += -(_grad_disp_0_local_t(2) * _grad_rot_0_local_t(0) -
                                            _grad_disp_0_local_t(0) * _grad_rot_0_local_t(2)) *
                                          _Iy[_qp];
  }

  _total_disp_strain[_qp] = _total_rotation[0].transpose() * _mech_disp_strain_increment[_qp] +
                            _total_disp_strain_old[_qp];
  _total_rot_strain[_qp] =
      _total_rotation[0].transpose() * _mech_rot_strain_increment[_qp] + _total_rot_strain_old[_qp];

  for (unsigned int i = 0; i < _eigenstrain_names.size(); ++i)
  {
    _mech_disp_strain_increment[_qp] -=
        _total_rotation[0] * ((*_disp_eigenstrain[i])[_qp] - (*_disp_eigenstrain_old[i])[_qp]) *
        _area[_qp];
    _mech_rot_strain_increment[_qp] -=
        _total_rotation[0] * ((*_rot_eigenstrain[i])[_qp] - (*_rot_eigenstrain_old[i])[_qp]);
  }

  Real c1_paper = std::sqrt(_material_stiffness[0](0));
  Real c2_paper = std::sqrt(_material_stiffness[0](1));

  Real effec_stiff_1 = std::max(c1_paper, c2_paper);

  Real effec_stiff_2 = 2 / (c2_paper * std::sqrt(A_avg / Iz_avg));

  _effective_stiffness[_qp] = std::max(effec_stiff_1, _original_length[0] / effec_stiff_2);

  if (_prefactor_function)
    _effective_stiffness[_qp] *= std::sqrt(_prefactor_function->value(_t, _q_point[_qp]));
}

template <bool is_ad>
void
ComputeIncrementalBeamStrainTempl<is_ad>::computeStiffnessMatrix()
{
  // only needed for non-AD analytic Jacobian path
  if constexpr (!is_ad)
  {
    const Real youngs_modulus = _material_stiffness[0](0);
    const Real shear_modulus = _material_stiffness[0](1);

    const Real A_avg = (_area[0] + _area[1]) / 2.0;
    const Real Iy_avg = (_Iy[0] + _Iy[1]) / 2.0;
    const Real Iz_avg = (_Iz[0] + _Iz[1]) / 2.0;
    Real Ix_avg = (_Ix[0] + _Ix[1]) / 2.0;
    if (!_has_Ix)
      Ix_avg = Iy_avg + Iz_avg;

    // K = |K11 K12|
    //     |K21 K22|

    // relation between translational displacements at node 0 and translational forces at node 0
    RankTwoTensor K11_local;
    K11_local.zero();
    K11_local(0, 0) = youngs_modulus * A_avg / _original_length[0];
    K11_local(1, 1) = shear_modulus * A_avg / _original_length[0];
    K11_local(2, 2) = shear_modulus * A_avg / _original_length[0];
    _K11[0] = _total_rotation[0].transpose() * K11_local * _total_rotation[0];

    // relation between displacements at node 0 and rotational moments at node 0
    RankTwoTensor K21_local;
    K21_local.zero();
    K21_local(2, 1) = shear_modulus * A_avg * 0.5;
    K21_local(1, 2) = -shear_modulus * A_avg * 0.5;
    _K21[0] = _total_rotation[0].transpose() * K21_local * _total_rotation[0];

    // relation between rotations at node 0 and rotational moments at node 0
    RankTwoTensor K22_local;
    K22_local.zero();
    K22_local(0, 0) = shear_modulus * Ix_avg / _original_length[0];
    K22_local(1, 1) = youngs_modulus * Iz_avg / _original_length[0] +
                      shear_modulus * A_avg * _original_length[0] / 4.0;
    K22_local(2, 2) = youngs_modulus * Iy_avg / _original_length[0] +
                      shear_modulus * A_avg * _original_length[0] / 4.0;
    _K22[0] = _total_rotation[0].transpose() * K22_local * _total_rotation[0];

    // relation between rotations at node 0 and rotational moments at node 1
    RankTwoTensor K22_local_cross = -K22_local;
    K22_local_cross(1, 1) += 2.0 * shear_modulus * A_avg * _original_length[0] / 4.0;
    K22_local_cross(2, 2) += 2.0 * shear_modulus * A_avg * _original_length[0] / 4.0;
    _K22_cross[0] = _total_rotation[0].transpose() * K22_local_cross * _total_rotation[0];

    // relation between displacements at node 0 and rotational moments at node 1
    _K21_cross[0] = -_K21[0];

    // stiffness matrix for large strain
    if (_large_strain)
    {
      // k1_large is the stiffness matrix obtained from sigma_xx * d(epsilon_xx)
      RankTwoTensor k1_large_11;
      // row 1
      k1_large_11(0, 0) = Utility::pow<2>(_grad_disp_0_local_t(0)) +
                          1.5 * Utility::pow<2>(_grad_rot_0_local_t(2)) * Iy_avg +
                          1.5 * Utility::pow<2>(_grad_rot_0_local_t(1)) * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(1)) +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(2)) +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(0)) * Ix_avg;
      k1_large_11(1, 0) = 0.5 * _grad_disp_0_local_t(0) * _grad_disp_0_local_t(1) -
                          1.0 / 3.0 * _grad_rot_0_local_t(0) * _grad_rot_0_local_t(1) * Iz_avg;
      k1_large_11(2, 0) = 0.5 * _grad_disp_0_local_t(0) * _grad_disp_0_local_t(2) -
                          1.0 / 3.0 * _grad_rot_0_local_t(0) * _grad_rot_0_local_t(2) * Iy_avg;

      // row 2
      k1_large_11(0, 1) = k1_large_11(1, 0);
      k1_large_11(1, 1) = Utility::pow<2>(_grad_disp_0_local_t(1)) +
                          1.5 * Utility::pow<2>(_grad_rot_0_local_t(0)) * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(0)) +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(2)) +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(2)) * Iy_avg +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(1)) * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(0)) * Iy_avg;
      k1_large_11(2, 1) = 0.5 * _grad_disp_0_local_t(1) * _grad_disp_0_local_t(2);

      // row 3
      k1_large_11(0, 2) = k1_large_11(2, 0);
      k1_large_11(1, 2) = k1_large_11(2, 1);
      k1_large_11(2, 2) = Utility::pow<2>(_grad_disp_0_local_t(2)) +
                          1.5 * Utility::pow<2>(_grad_rot_0_local_t(0)) * Iy_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(0)) +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(1)) +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(0)) * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(2)) * Iy_avg +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(2)) * Iz_avg;

      k1_large_11 *= 1.0 / 4.0 / Utility::pow<2>(_original_length[0]);

      RankTwoTensor k1_large_21;
      // row 1
      k1_large_21(0, 0) = 0.5 * _grad_disp_0_local_t(0) * _grad_rot_0_local_t(0) * (Ix_avg)-1.0 /
                              3.0 * _grad_disp_0_local_t(1) * _grad_rot_0_local_t(1) * Iz_avg -
                          1.0 / 3.0 * _grad_disp_0_local_t(2) * _grad_rot_0_local_t(2);
      k1_large_21(1, 0) = 1.5 * _grad_disp_0_local_t(0) * _grad_rot_0_local_t(1) * Iz_avg -
                          1.0 / 3.0 * _grad_disp_0_local_t(1) * _grad_rot_0_local_t(0) * Iz_avg;
      k1_large_21(2, 0) = 1.5 * _grad_disp_0_local_t(0) * _grad_rot_0_local_t(2) * Iy_avg -
                          1.0 / 3.0 * _grad_disp_0_local_t(2) * _grad_rot_0_local_t(0) * Iy_avg;

      // row 2
      k1_large_21(0, 1) = k1_large_21(1, 0);
      k1_large_21(1, 1) = 0.5 * _grad_disp_0_local_t(1) * _grad_rot_0_local_t(1) * Iz_avg -
                          1.0 / 3.0 * _grad_disp_0_local_t(0) * _grad_rot_0_local_t(0) * Iz_avg;
      k1_large_21(2, 1) = 0.5 * _grad_disp_0_local_t(1) * _grad_rot_0_local_t(2) * Iy_avg;

      // row 3
      k1_large_21(0, 2) = k1_large_21(2, 0);
      k1_large_21(1, 2) = k1_large_21(2, 1);
      k1_large_21(2, 2) = 0.5 * _grad_disp_0_local_t(2) * _grad_rot_0_local_t(2) * Iy_avg -
                          1.0 / 3.0 * _grad_disp_0_local_t(0) * _grad_rot_0_local_t(0) * Iy_avg;
      k1_large_21 *= 1.0 / 4.0 / Utility::pow<2>(_original_length[0]);

      RankTwoTensor k1_large_22;
      // row 1
      k1_large_22(0, 0) = Utility::pow<2>(_grad_rot_0_local_t(0)) * Utility::pow<2>(Ix_avg) +
                          1.5 * Utility::pow<2>(_grad_disp_0_local_t(1)) * Iz_avg +
                          1.5 * Utility::pow<2>(_grad_disp_0_local_t(2)) * Iy_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(0)) * Ix_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(2)) * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(1)) * Iy_avg +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(2)) * Iy_avg * Ix_avg +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(1)) * Iz_avg * Ix_avg;
      k1_large_22(1, 0) = 0.5 * _grad_rot_0_local_t(0) * _grad_rot_0_local_t(1) * Iz_avg * Ix_avg -
                          1.0 / 3.0 * _grad_disp_0_local_t(0) * _grad_disp_0_local_t(1) * Iz_avg;
      k1_large_22(2, 0) = 0.5 * _grad_rot_0_local_t(0) * _grad_rot_0_local_t(2) * Iy_avg * Ix_avg -
                          1.0 / 3.0 * _grad_disp_0_local_t(0) * _grad_disp_0_local_t(2) * Iy_avg;

      // row 2
      k1_large_22(0, 1) = k1_large_22(1, 0);
      k1_large_22(1, 1) = Utility::pow<2>(_grad_rot_0_local_t(1)) * Iz_avg * Iz_avg +
                          1.5 * Utility::pow<2>(_grad_disp_0_local_t(0)) * Iz_avg +
                          1.5 * Utility::pow<2>(_grad_rot_0_local_t(2)) * Iy_avg * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(1)) * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(2)) * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(0)) * Iz_avg * Ix_avg;
      k1_large_22(2, 1) = 1.5 * _grad_rot_0_local_t(1) * _grad_rot_0_local_t(2) * Iy_avg * Iz_avg;

      // row 3
      k1_large_22(0, 2) = k1_large_22(2, 0);
      k1_large_22(1, 2) = k1_large_22(2, 1);
      k1_large_22(2, 2) = Utility::pow<2>(_grad_rot_0_local_t(2)) * Iy_avg * Iy_avg +
                          1.5 * Utility::pow<2>(_grad_disp_0_local_t(0)) * Iy_avg +
                          1.5 * Utility::pow<2>(_grad_rot_0_local_t(1)) * Iy_avg * Iz_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(1)) * Iy_avg +
                          0.5 * Utility::pow<2>(_grad_disp_0_local_t(2)) * Iy_avg +
                          0.5 * Utility::pow<2>(_grad_rot_0_local_t(0)) * Iz_avg * Ix_avg;

      k1_large_22 *= 1.0 / 4.0 / Utility::pow<2>(_original_length[0]);

      // k2_large and k3_large are contributions from tau_xy * d(gamma_xy) and tau_xz * d(gamma_xz)
      // k2_large for node 1 is negative of that for node 0
      RankTwoTensor k2_large_11;
      // col 1
      k2_large_11(0, 0) =
          0.25 * Utility::pow<2>(_avg_rot_local_t(2)) + 0.25 * Utility::pow<2>(_avg_rot_local_t(1));
      k2_large_11(1, 0) = -1.0 / 6.0 * _avg_rot_local_t(0) * _avg_rot_local_t(1);
      k2_large_11(2, 0) = -1.0 / 6.0 * _avg_rot_local_t(0) * _avg_rot_local_t(2);

      // col 2
      k2_large_11(0, 1) = k2_large_11(1, 0);
      k2_large_11(1, 1) = 0.25 * _avg_rot_local_t(0);

      // col 3
      k2_large_11(0, 2) = k2_large_11(2, 0);
      k2_large_11(2, 2) = 0.25 * Utility::pow<2>(_avg_rot_local_t(0));

      k2_large_11 *= 1.0 / 4.0 / Utility::pow<2>(_original_length[0]);

      RankTwoTensor k2_large_22;
      // col1
      k2_large_22(0, 0) = 0.25 * Utility::pow<2>(_avg_rot_local_t(0)) * Ix_avg;
      k2_large_22(1, 0) = 1.0 / 6.0 * _avg_rot_local_t(0) * _avg_rot_local_t(1) * Iz_avg;
      k2_large_22(2, 0) = 1.0 / 6.0 * _avg_rot_local_t(0) * _avg_rot_local_t(2) * Iy_avg;

      // col2
      k2_large_22(0, 1) = k2_large_22(1, 0);
      k2_large_22(1, 1) = 0.25 * Utility::pow<2>(_avg_rot_local_t(2)) * Iz_avg +
                          0.25 * Utility::pow<2>(_avg_rot_local_t(1)) * Iz_avg;

      // col3
      k2_large_22(0, 2) = k2_large_22(2, 0);
      k2_large_22(2, 2) = 0.25 * Utility::pow<2>(_avg_rot_local_t(2)) * Iy_avg +
                          0.25 * Utility::pow<2>(_avg_rot_local_t(1)) * Iy_avg;

      k2_large_22 *= 1.0 / 4.0 / Utility::pow<2>(_original_length[0]);

      // k3_large for node 1 is same as that for node 0
      RankTwoTensor k3_large_22;
      // col1
      k3_large_22(0, 0) = 0.25 * Utility::pow<2>(_grad_disp_0_local_t(2)) +
                          0.25 * _grad_rot_0_local_t(0) * Ix_avg +
                          0.25 * Utility::pow<2>(_grad_disp_0_local_t(1));
      k3_large_22(1, 0) = -1.0 / 6.0 * _grad_disp_0_local_t(0) * _grad_disp_0_local_t(1) +
                          1.0 / 6.0 * _grad_rot_0_local_t(0) * _grad_rot_0_local_t(1) * Iz_avg;
      k3_large_22(2, 0) = -1.0 / 6.0 * _grad_disp_0_local_t(0) * _grad_disp_0_local_t(2) +
                          1.0 / 6.0 * _grad_rot_0_local_t(0) * _grad_rot_0_local_t(2) * Iy_avg;

      // col2
      k3_large_22(0, 1) = k3_large_22(1, 0);
      k3_large_22(2, 2) = 0.25 * Utility::pow<2>(_grad_disp_0_local_t(0)) +
                          0.25 * _grad_rot_0_local_t(2) * Iy_avg +
                          0.25 * _grad_rot_0_local_t(1) * Iz_avg;

      // col3
      k3_large_22(0, 2) = k3_large_22(2, 0);
      k3_large_22(2, 2) = 0.25 * Utility::pow<2>(_grad_disp_0_local_t(0)) +
                          0.25 * _grad_rot_0_local_t(2) * Iy_avg +
                          0.25 * _grad_rot_0_local_t(1) * Iz_avg;

      k3_large_22 *= 1.0 / 16.0;

      RankTwoTensor k3_large_21;
      // col1
      k3_large_21(0, 0) = -1.0 / 6.0 *
                          (_grad_disp_0_local_t(2) * _avg_rot_local_t(2) +
                           _grad_disp_0_local_t(1) * _avg_rot_local_t(1));
      k3_large_21(1, 0) = 0.25 * _grad_disp_0_local_t(0) * _avg_rot_local_t(1) -
                          1.0 / 6.0 * _grad_disp_0_local_t(1) * _avg_rot_local_t(0);
      k3_large_21(2, 0) = 0.25 * _grad_disp_0_local_t(0) * _avg_rot_local_t(2) -
                          1.0 / 6.0 * _grad_disp_0_local_t(2) * _avg_rot_local_t(0);

      // col2
      k3_large_21(0, 1) = 0.25 * _grad_disp_0_local_t(1) * _avg_rot_local_t(0) -
                          1.0 / 6.0 * _grad_disp_0_local_t(0) * _avg_rot_local_t(1);
      k3_large_21(1, 1) = -1.0 / 6.0 * _grad_disp_0_local_t(0) * _avg_rot_local_t(0);

      // col3
      k3_large_21(0, 2) = 0.25 * _grad_disp_0_local_t(2) * _avg_rot_local_t(0) -
                          1.0 / 6.0 * _grad_disp_0_local_t(0) * _avg_rot_local_t(2);
      k3_large_21(2, 2) = -1.0 / 6.0 * _grad_disp_0_local_t(0) * _avg_rot_local_t(0);

      k3_large_21 *= 1.0 / 8.0 / _original_length[0];

      RankTwoTensor k4_large_22;
      // col 1
      k4_large_22(0, 0) = 0.25 * _grad_rot_0_local_t(0) * _avg_rot_local_t(0) * Ix_avg +
                          1.0 / 6.0 * _grad_rot_0_local_t(2) * _avg_rot_local_t(2) * Iy_avg +
                          1.0 / 6.0 * _grad_rot_0_local_t(1) * _avg_rot_local_t(1) * Iz_avg;
      k4_large_22(1, 0) = 1.0 / 6.0 * _grad_rot_0_local_t(1) * _avg_rot_local_t(0) * Iz_avg;
      k4_large_22(2, 0) = 1.0 / 6.0 * _grad_rot_0_local_t(2) * _avg_rot_local_t(0) * Iy_avg;

      // col2
      k4_large_22(0, 1) = 1.0 / 6.0 * _grad_rot_0_local_t(0) * _avg_rot_local_t(1) * Iz_avg;
      k4_large_22(1, 1) = 0.25 * _grad_rot_0_local_t(1) * _avg_rot_local_t(1) * Iz_avg +
                          1.0 / 6.0 * _grad_rot_0_local_t(0) * _avg_rot_local_t(0) * Iz_avg;
      k4_large_22(2, 1) = 0.25 * _grad_rot_0_local_t(1) * _avg_rot_local_t(2) * Iz_avg;

      // col 3
      k4_large_22(0, 2) = 1.0 / 6.0 * _grad_rot_0_local_t(0) * _avg_rot_local_t(2) * Iy_avg;
      k4_large_22(1, 2) = 0.25 * _grad_rot_0_local_t(2) * _avg_rot_local_t(1) * Iy_avg;
      k4_large_22(2, 2) = 0.25 * _grad_rot_0_local_t(2) * _avg_rot_local_t(2) * Iy_avg +
                          1.0 / 6.0 * _grad_rot_0_local_t(0) * _avg_rot_local_t(0) * Iy_avg;

      k3_large_22 += 1.0 / 8.0 / _original_length[0] * (k4_large_22 + k4_large_22.transpose());

      // Assembling final matrix
      _K11[0] += _total_rotation[0].transpose() * (k1_large_11 + k2_large_11) * _total_rotation[0];
      _K22[0] += _total_rotation[0].transpose() * (k1_large_22 + k2_large_22 + k3_large_22) *
                 _total_rotation[0];
      _K21[0] += _total_rotation[0].transpose() * (k1_large_21 + k3_large_21) * _total_rotation[0];
      _K21_cross[0] +=
          _total_rotation[0].transpose() * (-k1_large_21 + k3_large_21) * _total_rotation[0];
      _K22_cross[0] += _total_rotation[0].transpose() * (-k1_large_22 - k2_large_22 + k3_large_22) *
                       _total_rotation[0];
    }
  }
}

template <bool is_ad>
void
ComputeIncrementalBeamStrainTempl<is_ad>::computeRotation()
{
  _total_rotation[0] = _original_local_config;
}

template class ComputeIncrementalBeamStrainTempl<false>;
template class ComputeIncrementalBeamStrainTempl<true>;