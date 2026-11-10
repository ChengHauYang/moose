//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ElementIntegralPostprocessor.h"

// Forward Declarations

/**
 * Computes one homogenized elastic-constant component from asymptotic-expansion correctors and
 * the local elasticity tensor. The AD form reads AD tensor properties but returns a scalar value.
 */
template <bool is_ad>
class AsymptoticExpansionHomogenizationElasticConstantsTempl
  : public ElementIntegralPostprocessor
{
public:
  static InputParameters validParams();

  AsymptoticExpansionHomogenizationElasticConstantsTempl(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void execute() override;
  virtual Real getValue() const override;
  virtual void finalize() override;
  virtual void threadJoin(const UserObject & y) override;

protected:
  virtual Real computeQpIntegral() override;

private:
  /// Base name of the material system
  const std::string _base_name;

  const std::array<std::array<const VariableGradient *, 3>, 6> _grad;

  const GenericMaterialProperty<RankFourTensor, is_ad> & _elasticity_tensor;
  const unsigned int _column, _row;
  const std::array<unsigned int, 6> _ik_index;
  const std::array<unsigned int, 6> _jl_index;
  const unsigned _i, _j, _k, _l;
  Real _volume;
  Real _integral_value;
};

using AsymptoticExpansionHomogenizationElasticConstants =
    AsymptoticExpansionHomogenizationElasticConstantsTempl<false>;
using ADAsymptoticExpansionHomogenizationElasticConstants =
    AsymptoticExpansionHomogenizationElasticConstantsTempl<true>;
