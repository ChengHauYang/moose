//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ADKernelValue.h" // Provides AD(Vector)KernelValue

/**
 * Computes the electro-hydrodynamic (EHD) body force
 *   \f$\vec{F} = \rho_e \vec{E} = \bigl(\sum_i z_i c_i\bigr)(-\nabla \phi)\f$
 * for an arbitrary number of ionic species and applies it to the momentum
 * equation. The kernel returns \f$-\vec{F}\f$ because it lives on the LHS
 * (\f$M - F = 0\f$).
 */
class INSADMomentumEHDForce : public ADVectorKernelValue
{
public:
  INSADMomentumEHDForce(const InputParameters & parameters);
  static InputParameters validParams();

protected:
  ADRealVectorValue precomputeQpResidual() override;

  /// Number of coupled ionic species (== _concentrations.size() == _valences.size()).
  const std::size_t _n_species;

  /// Coupled concentration values, one entry per species.
  std::vector<const ADVariableValue *> _concentrations;

  /// Valence (charge number) z_i for each species; matches _concentrations order.
  const std::vector<Real> _valences;

  /// Gradient of the electric potential.
  const ADVariableGradient & _grad_phi;
};
