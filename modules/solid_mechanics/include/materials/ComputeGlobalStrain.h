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

#include "GlobalStrainUserObjectInterface.h"

/**
 * ComputeGlobalStrain calculates the global strain tensor from the scalar variables
 */
template <bool is_ad>
class ComputeGlobalStrainTempl : public Material
{
public:
  static InputParameters validParams();

  ComputeGlobalStrainTempl(const InputParameters & parameters);

protected:
  virtual void initQpStatefulProperties() override;

  virtual void computeQpProperties() override;

  /// Base name prepended to material property name
  const std::string _base_name;
  const GenericVariableValue<is_ad> & _scalar_global_strain;
  GenericMaterialProperty<RankTwoTensor, is_ad> & _global_strain;

  const GlobalStrainUserObjectInterfaceTempl<is_ad> & _pst;
  const VectorValue<bool> & _periodic_dir;

  const unsigned int _dim;

  /// Number of displacement variables
  const unsigned int _ndisp;
};

using ComputeGlobalStrain = ComputeGlobalStrainTempl<false>;
using ADComputeGlobalStrain = ComputeGlobalStrainTempl<true>;
