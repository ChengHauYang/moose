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
#include "MooseTypes.h"

#include "RankTwoTensor.h"
#include "SymmetricRankTwoTensor.h"

#include "RankFourTensor.h"
#include "SymmetricRankFourTensor.h"

template <typename T, bool symmetrize>
struct MandelConversion
{
  using from = std::false_type;
  using to = std::false_type;
};
template <>
struct MandelConversion<RankTwoTensor, true>
{
  using from = RankTwoTensor;
  using to = SymmetricRankTwoTensor;
};
template <>
struct MandelConversion<RankTwoTensor, false>
{
  using from = SymmetricRankTwoTensor;
  using to = RankTwoTensor;
};
template <>
struct MandelConversion<RankFourTensor, true>
{
  using from = RankFourTensor;
  using to = SymmetricRankFourTensor;
};
template <>
struct MandelConversion<RankFourTensor, false>
{
  using from = SymmetricRankFourTensor;
  using to = RankFourTensor;
};

/**
 * Converts rank-two or rank-four material properties between full and Mandel notation.
 * `symmetrize` selects full-to-Mandel or Mandel-to-full conversion, while `is_ad` selects
 * whether the converted property retains automatic-differentiation derivatives.
 */
template <typename T, bool symmetrize, bool is_ad>
class MandelConverter : public Material
{
public:
  static InputParameters validParams();

  MandelConverter(const InputParameters & parameters);

  using FromType = typename MandelConversion<T, symmetrize>::from;
  using ToType = typename MandelConversion<T, symmetrize>::to;

protected:
  void initQpStatefulProperties() override;
  void computeQpProperties() override;

  const GenericMaterialProperty<FromType, is_ad> & _from;
  GenericMaterialProperty<ToType, is_ad> & _to;
};

using RankTwoTensorToSymmetricRankTwoTensor = MandelConverter<RankTwoTensor, true, false>;
using ADRankTwoTensorToSymmetricRankTwoTensor = MandelConverter<RankTwoTensor, true, true>;
using SymmetricRankTwoTensorToRankTwoTensor = MandelConverter<RankTwoTensor, false, false>;
using ADSymmetricRankTwoTensorToRankTwoTensor = MandelConverter<RankTwoTensor, false, true>;
using RankFourTensorToSymmetricRankFourTensor = MandelConverter<RankFourTensor, true, false>;
using ADRankFourTensorToSymmetricRankFourTensor = MandelConverter<RankFourTensor, true, true>;
using SymmetricRankFourTensorToRankFourTensor = MandelConverter<RankFourTensor, false, false>;
using ADSymmetricRankFourTensorToRankFourTensor = MandelConverter<RankFourTensor, false, true>;
