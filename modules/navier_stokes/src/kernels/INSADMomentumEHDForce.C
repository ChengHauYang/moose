//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "INSADMomentumEHDForce.h"
#include "INSADObjectTracker.h"
#include "FEProblemBase.h"

registerMooseObject("NavierStokesApp", INSADMomentumEHDForce);

InputParameters
INSADMomentumEHDForce::validParams()
{
  InputParameters params = ADVectorKernelValue::validParams();
  params.addClassDescription(
      "Computes the electro-hydrodynamic (EHD) body force "
      "F = (sum_i z_i c_i) * (-grad phi) and applies it to the momentum equation.");
  params.addRequiredCoupledVar(
      "concentrations",
      "Ionic species concentrations c_i. One coupled variable per species; the order "
      "must match the 'valences' parameter.");
  params.addRequiredParam<std::vector<Real>>(
      "valences",
      "Valence (signed charge number) z_i of each species. Must have the same length "
      "as 'concentrations'.");
  params.addRequiredCoupledVar("potential", "Electric potential phi.");
  return params;
}

INSADMomentumEHDForce::INSADMomentumEHDForce(const InputParameters & parameters)
  : ADVectorKernelValue(parameters),
    _n_species(coupledComponents("concentrations")),
    _valences(getParam<std::vector<Real>>("valences")),
    _grad_phi(adCoupledGradient("potential"))
{
  if (_valences.size() != _n_species)
    paramError("valences",
               "The number of valences (",
               _valences.size(),
               ") must match the number of coupled concentrations (",
               _n_species,
               ").");

  _concentrations.reserve(_n_species);
  for (const auto i : make_range(_n_species))
    _concentrations.push_back(&adCoupledValue("concentrations", i));

  // Preserve the existing INSAD tracker hook so INSADMaterial knows a body force
  // exists on these blocks. VMS/SUPG stabilization of the EHD term itself is a
  // separate concern and is intentionally left as-is.
  auto & obj_tracker = const_cast<INSADObjectTracker &>(
      _fe_problem.getUserObject<INSADObjectTracker>("ins_ad_object_tracker"));
  for (const auto block_id : blockIDs())
    obj_tracker.set("has_coupled_force", true, block_id);
}

ADRealVectorValue
INSADMomentumEHDForce::precomputeQpResidual()
{
  // charge_density = sum_i z_i * c_i(qp)
  ADReal charge_density = 0;
  for (const auto i : make_range(_n_species))
    charge_density += _valences[i] * (*_concentrations[i])[_qp];

  // Physical force: F = charge_density * (-grad phi)
  // Kernel is on LHS (M - F = 0), so we return -F = charge_density * grad phi.
  return charge_density * _grad_phi[_qp];
}
