//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MOOSEToNEML2SideBatched.h"

#include "RankTwoTensor.h"
#include "SymmetricRankTwoTensor.h"

/**
 * Gather a MOOSE material property on sidesets for insertion into the NEML2 model.
 *
 * This is similar to MOOSEMaterialPropertyToNEML2, but is designed to work on sidesets and gather
 * data into side batches.
 */
template <typename T, unsigned int state>
class MOOSESideMaterialPropertyToNEML2 : public MOOSEToNEML2SideBatched<T>
{
public:
  static InputParameters validParams();

  MOOSESideMaterialPropertyToNEML2(const InputParameters & params);

#ifdef NEML2_ENABLED
protected:
  const MooseArray<T> & sideMOOSEData() const override { return _mat_prop.get(); }

  /// MOOSE material property to read data from
  const MaterialProperty<T> & _mat_prop;
#endif
};

#define DefineMOOSESideMaterialPropertyToNEML2Alias(T, alias)                                      \
  using MOOSESide##alias##MaterialPropertyToNEML2 = MOOSESideMaterialPropertyToNEML2<T, 0>;        \
  using MOOSEOldSide##alias##MaterialPropertyToNEML2 = MOOSESideMaterialPropertyToNEML2<T, 1>

DefineMOOSESideMaterialPropertyToNEML2Alias(Real, Real);
DefineMOOSESideMaterialPropertyToNEML2Alias(RankTwoTensor, RankTwoTensor);
DefineMOOSESideMaterialPropertyToNEML2Alias(SymmetricRankTwoTensor, SymmetricRankTwoTensor);
DefineMOOSESideMaterialPropertyToNEML2Alias(RealVectorValue, RealVectorValue);
