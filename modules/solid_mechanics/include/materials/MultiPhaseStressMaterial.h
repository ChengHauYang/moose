//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "Material.h"
#include "RankTwoTensorForward.h"
#include "RankFourTensorForward.h"

/**
 * Construct a global strain from the phase strains in a manner that is consistent
 * with the construction of the global elastic energy by DerivativeMultiPhaseMaterial.
 */
template <bool is_ad>
class MultiPhaseStressMaterialTempl : public Material
{
public:
  static InputParameters validParams();

  MultiPhaseStressMaterialTempl(const InputParameters & parameters);

protected:
  virtual void computeQpProperties();

  /// switching function name list
  std::vector<MaterialPropertyName> _h_list;

  /// number of phases handled by this material
  unsigned int _n_phase;

  /// switching functions
  std::vector<const GenericMaterialProperty<Real, is_ad> *> _h_eta;

  // phase material properties
  std::vector<std::string> _phase_base;
  std::vector<const GenericMaterialProperty<RankTwoTensor, is_ad> *> _phase_stress;
  std::vector<const MaterialProperty<RankFourTensor> *> _dphase_stress_dstrain;

  // global material properties

  /// Base name of the material system
  const std::string _base_name;

  /// The stress tensor
  GenericMaterialProperty<RankTwoTensor, is_ad> & _stress;

  /// Non-AD tangent; AD consumers differentiate the stress property directly.
  MaterialProperty<RankFourTensor> * const _dstress_dstrain;
};

using MultiPhaseStressMaterial = MultiPhaseStressMaterialTempl<false>;
using ADMultiPhaseStressMaterial = MultiPhaseStressMaterialTempl<true>;
