//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GenericKernel.h"

class Function;

/**
 * MaterialVectorBodyForce applies a body force (force/volume) given as a vector material property
 */
template <bool is_ad>
class MaterialVectorBodyForceTempl : public GenericKernel<is_ad>
{
public:
  static InputParameters validParams();

  MaterialVectorBodyForceTempl(const InputParameters & parameters);

protected:
  virtual GenericReal<is_ad> computeQpResidual() override;

  /// coordinate axis this Kernel acts on
  unsigned int _component;

  /// coupled body force vector property
  const GenericMaterialProperty<RealVectorValue, is_ad> & _body_force;

  /// optional scaling function
  const Function & _function;

  // alpha parameter for HHT time integration scheme
  const Real _alpha;

  usingGenericKernelMembers;
};

using MaterialVectorBodyForce = MaterialVectorBodyForceTempl<false>;
using ADMaterialVectorBodyForce = MaterialVectorBodyForceTempl<true>;
