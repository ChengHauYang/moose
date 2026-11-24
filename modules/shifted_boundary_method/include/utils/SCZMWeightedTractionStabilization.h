//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MooseObject.h"
#include "MooseEnum.h"

/**
 * Weighted traction stabilization of the SCZM interface flux.
 *
 * The cohesive traction t_c in the interface residual is replaced by
 *   t_hat = S t_c + (I - S) T_h,   S = beta (K + beta I)^{-1},
 * where K = k_t I + (k_n - k_t) N (x) N is a reference cohesive stiffness, T_h = 1/2 (P^- + P^+) N
 * is the average bulk stress traction on the true normal N (the surrogate normal when unshifted),
 * and beta ~ gamma E / h is the Nitsche penalty. The cohesive contribution to the Jacobian is then
 * S K_c, bounded by beta I, so a stiff cohesive law no longer destroys the definiteness of the
 * shifted interface terms. With N the unit normal, S has the eigenvalues
 *   S_n = beta / (k_n + beta) along N and S_t = beta / (k_t + beta) in the tangent plane.
 */
namespace SCZMWeightedTractionStabilization
{
inline void
addParams(InputParameters & params)
{
  params.addParam<MooseEnum>(
      "stabilization",
      MooseEnum("none weighted_traction", "none"),
      "Interface flux stabilization. 'weighted_traction' blends the cohesive traction with the "
      "average bulk stress traction, t = S t_c + (I - S) (P^- + P^+) N / 2 with "
      "S = beta (K + beta I)^-1, which caps the cohesive stiffness seen by the Jacobian at beta.");
  params.addRangeCheckedParam<Real>(
      "stabilization_beta",
      "stabilization_beta > 0",
      "Penalty beta of the weighted traction stabilization, typically gamma E / h with E the bulk "
      "Young's modulus and h the element size.");
  params.addRangeCheckedParam<Real>(
      "stabilization_normal_stiffness",
      "stabilization_normal_stiffness >= 0",
      "Reference normal cohesive stiffness k_n defining the weight S of the weighted traction "
      "stabilization.");
  params.addRangeCheckedParam<Real>(
      "stabilization_tangential_stiffness",
      "stabilization_tangential_stiffness >= 0",
      "Reference tangential cohesive stiffness k_t defining the weight S of the weighted traction "
      "stabilization. Defaults to 'stabilization_normal_stiffness'.");
}

/// The eigenvalues of the stabilization weight S, or both zero when the stabilization is off
struct Weights
{
  bool enabled = false;
  /// S_n = beta / (k_n + beta), along the normal
  Real normal = 0;
  /// S_t = beta / (k_t + beta), in the tangent plane
  Real tangential = 0;
};

inline Weights
weights(const MooseObject & object)
{
  Weights result;
  result.enabled = object.getParam<MooseEnum>("stabilization") == "weighted_traction";
  if (!result.enabled)
  {
    for (const auto & name : {"stabilization_beta",
                              "stabilization_normal_stiffness",
                              "stabilization_tangential_stiffness"})
      if (object.isParamSetByUser(name))
        object.paramError(name, "This parameter requires 'stabilization = weighted_traction'.");
    return result;
  }

  for (const auto & name : {"stabilization_beta", "stabilization_normal_stiffness"})
    if (!object.isParamValid(name))
      object.paramError(name, "This parameter is required with 'stabilization = weighted_traction'.");

  const auto beta = object.getParam<Real>("stabilization_beta");
  const auto normal_stiffness = object.getParam<Real>("stabilization_normal_stiffness");
  const auto tangential_stiffness = object.isParamValid("stabilization_tangential_stiffness")
                                        ? object.getParam<Real>("stabilization_tangential_stiffness")
                                        : normal_stiffness;
  result.normal = beta / (normal_stiffness + beta);
  result.tangential = beta / (tangential_stiffness + beta);
  return result;
}
}
