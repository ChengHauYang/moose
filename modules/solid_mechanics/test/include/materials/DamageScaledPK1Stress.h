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

class DamageScaledPK1Stress : public Material
{
public:
  static InputParameters validParams();
  DamageScaledPK1Stress(const InputParameters & parameters);

protected:
  virtual void computeQpProperties() override;

  const std::string _base_name;
  const std::string _source_base_name;

  const VariableValue & _damage;

  const MaterialProperty<RankTwoTensor> & _pk1_undamaged;
  const MaterialProperty<RankFourTensor> & _dpk1_undamaged;

  MaterialProperty<RankTwoTensor> & _pk1;
  MaterialProperty<RankFourTensor> & _dpk1;
  MaterialProperty<RankTwoTensor> & _dpk1_ddamage;
};
