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
#include "RankTwoTensor.h"
#include "DerivativeMaterialInterface.h"

/**
 * SumTensorIncrements update a tensor by summing tensor increments passed as property
 */
template <bool is_ad>
class SumTensorIncrementsTempl : public DerivativeMaterialInterface<Material>
{
public:
  static InputParameters validParams();

  SumTensorIncrementsTempl(const InputParameters & parameters);

protected:
  void initQpStatefulProperties() override;
  void computeQpProperties() override;

  std::vector<MaterialPropertyName> _property_names;
  unsigned int _num_property;

  GenericMaterialProperty<RankTwoTensor, is_ad> & _tensor;
  const MaterialProperty<RankTwoTensor> & _tensor_old;
  GenericMaterialProperty<RankTwoTensor, is_ad> & _tensor_increment;

  std::vector<const GenericMaterialProperty<RankTwoTensor, is_ad> *>
      _coupled_tensor_increments;
};

using SumTensorIncrements = SumTensorIncrementsTempl<false>;
using ADSumTensorIncrements = SumTensorIncrementsTempl<true>;
