//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SalehaniIrani3DCTraction.h"

registerMooseObject("SolidMechanicsApp", SalehaniIrani3DCTraction);

InputParameters
SalehaniIrani3DCTraction::validParams()
{
  InputParameters params = CZMComputeLocalTractionTotalBase::validParams();
  params.addClassDescription("3D Coupled (3DC) cohesive law of Salehani and Irani with no damage");
  params.addRequiredParam<Real>(
      "normal_gap_at_maximum_normal_traction",
      "The value of normal gap at which maximum normal traction is achieved");
  params.addRequiredParam<Real>(
      "tangential_gap_at_maximum_shear_traction",
      "The value of tangential gap at which maximum shear traction is achieved");
  params.addRequiredParam<Real>("maximum_normal_traction",
                                "The maximum normal traction the interface can sustain");
  params.addRequiredParam<Real>("maximum_shear_traction",
                                "The maximum shear traction the interface can sustain");
  return params;
}

SalehaniIrani3DCTraction::SalehaniIrani3DCTraction(const InputParameters & parameters)
  : CZMComputeLocalTractionTotalBase(parameters),
    _delta_u0({getParam<Real>("normal_gap_at_maximum_normal_traction"),
               std::sqrt(2) * getParam<Real>("tangential_gap_at_maximum_shear_traction"),
               std::sqrt(2) * getParam<Real>("tangential_gap_at_maximum_shear_traction")}),
    _max_allowable_traction({getParam<Real>("maximum_normal_traction"),
                             getParam<Real>("maximum_shear_traction"),
                             getParam<Real>("maximum_shear_traction")})
{
}

void
SalehaniIrani3DCTraction::computeInterfaceTractionAndDerivatives()
{
  const auto result = computeTrialTraction(_interface_displacement_jump[_qp]);
  _interface_traction[_qp] = result.traction;
  _dinterface_traction_djump[_qp] = result.dtraction_djump;
}

SalehaniIrani3DCTraction::TrialTractionResult
SalehaniIrani3DCTraction::computeTrialTraction(const RealVectorValue & trial_jump) const
{
  // The convention for ordering the traction is N, T, S, where N is the normal direction, and T and
  // S are two arbitrary tangential directions.
  //
  // Partial derivatives in local coordinates wrt the displacement jump:
  // T_i = a_i * b_i * exp(-x)
  // a_i = sigma_i,max * (alpha_i * e)^{1 / alpha_i}, alpha_0 = 1, alpha_1 = alpha_2 = 2
  // b_i = trial_jump(i) / _delta_u0(i)
  // x = sum_i (trial_jump(i) / _delta_u0(i))^{alpha_i}
  // dT_i / du_j = a_i * exp(-x) * (db_i / du_j - b_i * dx / du_j)
  Real x = 0.0;
  for (const auto i : make_range(3u))
  {
    Real aa = trial_jump(i) / _delta_u0(i);
    if (i > 0)
      aa *= aa; // square for shear component
    x += aa;
  }

  const Real exp_x = std::exp(-x);
  const Real e = std::exp(1.0);
  const Real shear_factor = std::sqrt(2.0 * e);

  TrialTractionResult result;
  for (const auto i : make_range(3u))
  {
    const Real a_i = _max_allowable_traction(i) * (i == 0 ? e : shear_factor);
    const Real b_i = trial_jump(i) / _delta_u0(i);
    result.traction(i) = a_i * b_i * exp_x;

    for (const auto j : make_range(3u))
    {
      const Real dbi_duj = (i == j) ? 1.0 / _delta_u0(j) : 0.0;
      const Real dx_duj =
          (j == 0) ? 1.0 / _delta_u0(j) : 2.0 * trial_jump(j) / (_delta_u0(j) * _delta_u0(j));
      result.dtraction_djump(i, j) = a_i * exp_x * (dbi_duj - b_i * dx_duj);
    }
  }

  return result;
}
