//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

class KokkosSCZMStabilizationData;

/**
 * Host interface of a Kokkos traction-separation material whose cohesive law the weighted-traction
 * prepass can evaluate at trial jumps.
 *
 * Device code has no virtual dispatch, so the material supplies its own concrete type to the
 * templated prepass. An implementation provides
 *   - a KOKKOS_FUNCTION trialLaw(datum, qp) returning a per-QP local law with
 *     CohesiveEvaluation evaluate(const Real3 & local_jump, unsigned int ndisp) const and
 *     bool isAffine() const; history (old damage, old jump) is read inside trialLaw and the law
 *     itself must not mutate state;
 *   - robinPrepass(), defined in the material's .K file as { uo.robinPrepass(*this); }.
 */
class SCZMTrialTractionProvider
{
public:
  virtual ~SCZMTrialTractionProvider() = default;

  /// Solves the generalized Robin problem of uo at every interface QP with this material's law
  virtual void robinPrepass(const KokkosSCZMStabilizationData & uo) const = 0;

  /// Errors if uo's stabilization law parameters disagree with this material's law
  virtual void checkStabilizationLaw(const KokkosSCZMStabilizationData & /*uo*/) const {}
};
