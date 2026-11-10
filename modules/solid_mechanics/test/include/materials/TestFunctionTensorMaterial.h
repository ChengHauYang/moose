//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "Material.h"

/**
 * Test-only material that linearly interpolates or extrapolates between two constant tensors using
 * a coupled scalar variable \f$c\f$: \f$\mathbf{T}(c) = (1-c)\mathbf{T}_0 + c\mathbf{T}_1\f$.
 * It verifies propagation of derivatives from an AD RealTensorValue material property into
 * dependent objects.
 */
template <bool is_ad>
class TestFunctionTensorMaterialTempl : public Material
{
public:
  static InputParameters validParams();

  TestFunctionTensorMaterialTempl(const InputParameters & parameters);

protected:
  void computeQpProperties() override;

  const GenericVariableValue<is_ad> & _c;
  GenericMaterialProperty<RealTensorValue, is_ad> & _prop;

  /// Component values of tensor0 and tensor1 (row-major)
  const std::vector<Real> _tensor0;
  const std::vector<Real> _tensor1;

  using Material::_qp;
};

using TestFunctionTensorMaterial = TestFunctionTensorMaterialTempl<false>;
using ADTestFunctionTensorMaterial = TestFunctionTensorMaterialTempl<true>;
