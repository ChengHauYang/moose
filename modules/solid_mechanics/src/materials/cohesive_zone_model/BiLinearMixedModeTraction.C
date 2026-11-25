//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "BiLinearMixedModeTraction.h"
#include "libmesh/utility.h"

registerMooseObject("SolidMechanicsApp", BiLinearMixedModeTraction);

InputParameters
BiLinearMixedModeTraction::validParams()
{
  InputParameters params = CZMComputeLocalTractionTotalBase::validParams();
  params.addClassDescription("Mixed mode bilinear traction separation law.");
  params.addRequiredParam<Real>("penalty_stiffness", "Penalty stiffness.");
  params.addRequiredParam<MaterialPropertyName>(
      "GI_c", "Critical energy release rate in normal direction.");
  params.addRequiredParam<MaterialPropertyName>("GII_c",
                                                "Critical energy release rate in shear direction.");
  params.addRequiredParam<MaterialPropertyName>("normal_strength",
                                                "Tensile strength in normal direction.");
  params.addRequiredParam<MaterialPropertyName>("shear_strength",
                                                "Tensile strength in shear direction.");
  params.addRequiredParam<Real>("eta", "The power law parameter.");
  MooseEnum criterion("POWER_LAW BK", "BK");
  params.addParam<Real>("viscosity", 0.0, "Viscosity.");
  params.addParam<MooseEnum>(
      "mixed_mode_criterion", criterion, "Option for mixed mode propagation criterion.");

  // Advanced parameters to improve numerical convergence
  params.addParam<bool>(
      "lag_mode_mixity", true, "Whether to use old displacement jumps to compute the mode mixity.");
  params.addParam<bool>(
      "lag_displacement_jump",
      false,
      "Whether to use old displacement jumps to compute the effective displacement jump.");
  params.addParam<Real>("alpha", 1e-10, "Regularization parameter for the Macaulay bracket.");
  params.addParamNamesToGroup("lag_mode_mixity lag_displacement_jump alpha", "Advanced");

  return params;
}

BiLinearMixedModeTraction::BiLinearMixedModeTraction(const InputParameters & parameters)
  : CZMComputeLocalTractionTotalBase(parameters),
    _K(getParam<Real>("penalty_stiffness")),
    _d(declareProperty<Real>(_base_name + "damage")),
    _d_old(getMaterialPropertyOld<Real>(_base_name + "damage")),
    _interface_displacement_jump_old(
        getMaterialPropertyOld<RealVectorValue>(_base_name + "interface_displacement_jump")),
    _delta_init(
        declareProperty<Real>(_base_name + "effective_displacement_jump_at_damage_initiation")),
    _delta_final(
        declareProperty<Real>(_base_name + "effective_displacement_jump_at_full_degradation")),
    _delta_m(declareProperty<Real>(_base_name + "effective_displacement_jump")),
    _GI_c(getMaterialProperty<Real>(_base_name + getParam<MaterialPropertyName>("GI_c"))),
    _GII_c(getMaterialProperty<Real>(_base_name + getParam<MaterialPropertyName>("GII_c"))),
    _N(getMaterialProperty<Real>(_base_name + getParam<MaterialPropertyName>("normal_strength"))),
    _S(getMaterialProperty<Real>(_base_name + getParam<MaterialPropertyName>("shear_strength"))),
    _eta(getParam<Real>("eta")),
    _beta(declareProperty<Real>(_base_name + "mode_mixity_ratio")),
    _viscosity(getParam<Real>("viscosity")),
    _criterion(getParam<MooseEnum>("mixed_mode_criterion").getEnum<MixedModeCriterion>()),
    _lag_mode_mixity(getParam<bool>("lag_mode_mixity")),
    _lag_disp_jump(getParam<bool>("lag_displacement_jump")),
    _alpha(getParam<Real>("alpha"))
{
}

void
BiLinearMixedModeTraction::initQpStatefulProperties()
{
  CZMComputeLocalTractionTotalBase::initQpStatefulProperties();

  _d[_qp] = 0.0;
}

void
BiLinearMixedModeTraction::computeInterfaceTractionAndDerivatives()
{
  const auto result =
      computeTrialTraction(_interface_displacement_jump[_qp],
                           OldState{_d_old[_qp], _interface_displacement_jump_old[_qp]});
  _interface_traction[_qp] = result.traction;
  _dinterface_traction_djump[_qp] = result.dtraction_djump;
  _d[_qp] = result.damage;
  _beta[_qp] = result.beta;
  _delta_init[_qp] = result.delta_init;
  _delta_final[_qp] = result.delta_final;
  _delta_m[_qp] = result.delta_m;
}

BiLinearMixedModeTraction::TrialTractionResult
BiLinearMixedModeTraction::computeTrialTraction(const RealVectorValue & trial_jump,
                                                const OldState & old_state) const
{
  const RealVectorValue & mixity_delta =
      _lag_mode_mixity ? old_state.interface_displacement_jump : trial_jump;

  Real beta = 0.0;
  RealVectorValue dbeta_ddelta(0.0, 0.0, 0.0);
  if (mixity_delta(0) > 0.0)
  {
    const Real delta_s =
        std::sqrt(mixity_delta(1) * mixity_delta(1) + mixity_delta(2) * mixity_delta(2));
    beta = delta_s / mixity_delta(0);

    if (!_lag_mode_mixity && delta_s > 0.0)
      // delta_s is nonnegative; only exact zero needs special handling to avoid 0/0.
      dbeta_ddelta = RealVectorValue(-delta_s / mixity_delta(0) / mixity_delta(0),
                                     mixity_delta(1) / delta_s / mixity_delta(0),
                                     mixity_delta(2) / delta_s / mixity_delta(0));
  }

  const Real delta_normal0 = _N[_qp] / _K;
  const Real delta_shear0 = _S[_qp] / _K;

  Real delta_init = delta_shear0;
  RealVectorValue ddelta_init_ddelta(0.0, 0.0, 0.0);
  if (mixity_delta(0) > 0.0)
  {
    const Real delta_mixed =
        std::sqrt(delta_shear0 * delta_shear0 + Utility::pow<2>(beta * delta_normal0));
    delta_init = delta_normal0 * delta_shear0 * std::sqrt(1.0 + beta * beta) / delta_mixed;
    if (!_lag_mode_mixity)
    {
      const Real ddelta_init_dbeta =
          delta_init * beta *
          (1.0 / (1.0 + beta * beta) - Utility::pow<2>(delta_normal0 / delta_mixed));
      ddelta_init_ddelta = ddelta_init_dbeta * dbeta_ddelta;
    }
  }

  Real delta_final = 2.0 * _GII_c[_qp] / _S[_qp];
  RealVectorValue ddelta_final_ddelta(0.0, 0.0, 0.0);
  if (mixity_delta(0) > 0.0)
  {
    if (_criterion == MixedModeCriterion::BK)
    {
      delta_final = 2.0 / _K / delta_init *
                    (_GI_c[_qp] + (_GII_c[_qp] - _GI_c[_qp]) *
                                      std::pow(beta * beta / (1.0 + beta * beta), _eta));
      if (!_lag_mode_mixity)
      {
        const Real ddelta_final_ddelta_init = -delta_final / delta_init;
        const Real ddelta_final_dbeta =
            MooseUtils::absoluteFuzzyEqual(beta, 0.0)
                ? 0.0
                : 2.0 / _K / delta_init * (_GII_c[_qp] - _GI_c[_qp]) * _eta *
                      std::pow(beta * beta / (1.0 + beta * beta), _eta - 1.0) * 2.0 * beta /
                      Utility::pow<2>(1.0 + beta * beta);
        ddelta_final_ddelta =
            ddelta_final_ddelta_init * ddelta_init_ddelta + ddelta_final_dbeta * dbeta_ddelta;
      }
    }
    else if (_criterion == MixedModeCriterion::POWER_LAW)
    {
      const Real Gc_mixed =
          std::pow(1.0 / _GI_c[_qp], _eta) + std::pow(beta * beta / _GII_c[_qp], _eta);
      delta_final = (2.0 + 2.0 * beta * beta) / _K / delta_init * std::pow(Gc_mixed, -1.0 / _eta);
      if (!_lag_mode_mixity)
      {
        const Real ddelta_final_ddelta_init = -delta_final / delta_init;
        const Real ddelta_final_dbeta =
            delta_final * 2.0 * beta / (1.0 + beta * beta) -
            (2.0 + 2.0 * beta * beta) / _K / delta_init * std::pow(Gc_mixed, -1.0 / _eta - 1.0) *
                std::pow(beta * beta / _GII_c[_qp], _eta - 1.0) * 2.0 * beta / _GII_c[_qp];
        ddelta_final_ddelta =
            ddelta_final_ddelta_init * ddelta_init_ddelta + ddelta_final_dbeta * dbeta_ddelta;
      }
    }
  }

  const RealVectorValue & eff_delta =
      _lag_disp_jump ? old_state.interface_displacement_jump : trial_jump;

  const Real delta_normal_pos =
      MathUtils::regularizedHeavyside(eff_delta(0), _alpha) * eff_delta(0);
  const Real delta_m = std::sqrt(Utility::pow<2>(eff_delta(1)) + Utility::pow<2>(eff_delta(2)) +
                                 Utility::pow<2>(delta_normal_pos));
  RealVectorValue ddelta_m_ddelta(0.0, 0.0, 0.0);
  if (!_lag_disp_jump && !MooseUtils::absoluteFuzzyEqual(delta_m, 0.0))
  {
    const Real ddelta_normal_pos_ddelta_normal =
        MathUtils::regularizedHeavysideDerivative(eff_delta(0), _alpha) * eff_delta(0) +
        MathUtils::regularizedHeavyside(eff_delta(0), _alpha);
    ddelta_m_ddelta = RealVectorValue(
        delta_normal_pos * ddelta_normal_pos_ddelta_normal, eff_delta(1), eff_delta(2));
    ddelta_m_ddelta /= delta_m;
  }

  Real d = 0.0;
  if (delta_m < delta_init)
    d = 0.0;
  else if (delta_m > delta_final)
    d = 1.0;
  else
    d = delta_final * (delta_m - delta_init) / delta_m / (delta_final - delta_init);

  RealVectorValue dd_ddelta(0.0, 0.0, 0.0);
  if (d < old_state.damage)
    // Irreversibility
    d = old_state.damage;
  else if (delta_m >= delta_init && delta_m <= delta_final)
    dd_ddelta = (ddelta_final_ddelta * (delta_m - delta_init) +
                 delta_final * (ddelta_m_ddelta - ddelta_init_ddelta)) /
                    delta_m / (delta_final - delta_init) -
                delta_final * (delta_m - delta_init) *
                    (ddelta_m_ddelta * (delta_final - delta_init) +
                     delta_m * (ddelta_final_ddelta - ddelta_init_ddelta)) /
                    Utility::pow<2>(delta_m * (delta_final - delta_init));

  // Viscous regularization
  d = (d + _viscosity * old_state.damage / _dt) / (_viscosity / _dt + 1.0);
  dd_ddelta /= (_viscosity / _dt + 1.0);

  // Split displacement jump into active and inactive parts
  const RealVectorValue delta_active(std::max(trial_jump(0), 0.0), trial_jump(1), trial_jump(2));
  const RealVectorValue delta_inactive(std::min(trial_jump(0), 0.0), 0.0, 0.0);

  TrialTractionResult result;
  result.traction = (1.0 - d) * _K * delta_active + _K * delta_inactive;

  // The displacement jump split depends on the displacement jump, obviously
  RankTwoTensor ddelta_active_ddelta, ddelta_inactive_ddelta;
  ddelta_active_ddelta.fillFromInputVector({trial_jump(0) > 0.0 ? 1.0 : 0.0, 1.0, 1.0});
  ddelta_inactive_ddelta.fillFromInputVector({trial_jump(0) < 0.0 ? 1.0 : 0.0, 0.0, 0.0});

  result.dtraction_djump = (1.0 - d) * _K * ddelta_active_ddelta + _K * ddelta_inactive_ddelta;

  // The damage may also depend on the displacement jump
  RankTwoTensor A;
  A.vectorOuterProduct(delta_active, dd_ddelta);
  result.dtraction_djump -= _K * A;

  result.damage = d;
  result.beta = beta;
  result.delta_init = delta_init;
  result.delta_final = delta_final;
  result.delta_m = delta_m;

  return result;
}

RealVectorValue
BiLinearMixedModeTraction::computeTraction()
{
  computeDamage();

  // Split displacement jump into active and inactive parts
  const RealVectorValue delta = _interface_displacement_jump[_qp];
  const RealVectorValue delta_active(std::max(delta(0), 0.), delta(1), delta(2));
  const RealVectorValue delta_inactive(std::min(delta(0), 0.), 0, 0);

  return (1 - _d[_qp]) * _K * delta_active + _K * delta_inactive;
}

RankTwoTensor
BiLinearMixedModeTraction::computeTractionDerivatives()
{
  const auto result =
      computeTrialTraction(_interface_displacement_jump[_qp],
                           OldState{_d_old[_qp], _interface_displacement_jump_old[_qp]});
  return result.dtraction_djump;
}

void
BiLinearMixedModeTraction::computeDamage()
{
  const auto result =
      computeTrialTraction(_interface_displacement_jump[_qp],
                           OldState{_d_old[_qp], _interface_displacement_jump_old[_qp]});
  _d[_qp] = result.damage;
  _beta[_qp] = result.beta;
  _delta_init[_qp] = result.delta_init;
  _delta_final[_qp] = result.delta_final;
  _delta_m[_qp] = result.delta_m;
}
