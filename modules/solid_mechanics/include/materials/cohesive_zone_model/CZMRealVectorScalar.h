//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "InterfaceMaterial.h"
/**
 * Computes a scalar measure of a vector material property on a cohesive interface. The normal
 * option returns the normal component, while the tangent option returns the tangential magnitude;
 * use CZMRealVectorCartesianComponent to extract a Cartesian component instead.
 */
template <bool is_ad>
class CZMRealVectorScalarTempl : public InterfaceMaterial
{
public:
  static InputParameters validParams();

  CZMRealVectorScalarTempl(const InputParameters & parameters);

protected:
  void computeQpProperties() override;

  /// Base name of the material system
  const std::string _base_name;

  /// scalar options
  enum class DirectionType
  {
    Normal,
    Tangent
  } _direction;

  /// the property created by this material
  GenericMaterialProperty<Real, is_ad> & _property;

  /// the vector material property
  const GenericMaterialProperty<RealVectorValue, is_ad> & _vector;

  /// the material property defining the czm normal
  const GenericMaterialProperty<RankTwoTensor, is_ad> & _czm_rotation;

  usingInterfaceMaterialMembers;
};

using CZMRealVectorScalar = CZMRealVectorScalarTempl<false>;
using ADCZMRealVectorScalar = CZMRealVectorScalarTempl<true>;
