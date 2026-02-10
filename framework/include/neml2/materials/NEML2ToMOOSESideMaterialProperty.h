//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "NEML2Utils.h"
#include "Material.h"
#include "SymmetricRankTwoTensor.h"
#include "SymmetricRankFourTensor.h"

#include <optional>

#ifdef NEML2_ENABLED
#include "neml2/tensors/TensorBase.h"
#endif

class NEML2ModelExecutor;

template <typename T>
class NEML2ToMOOSESideMaterialProperty : public Material
{
public:
  static InputParameters validParams();

  NEML2ToMOOSESideMaterialProperty(const InputParameters & params);

#ifdef NEML2_ENABLED
  void computeProperties() override;

protected:
  void initQpStatefulProperties() override {}

  /// User object managing the execution of the NEML2 model
  const NEML2ModelExecutor & _execute_neml2_model;

  /// Emitted material property
  MaterialProperty<T> & _prop;

  /// Initial condition
  const MaterialProperty<T> * _prop0;

  /// Reference to the requested output (or its derivative) value
  const neml2::Tensor & _value;

  /// Requested NEML2 output variable name
  const neml2::VariableName _from_neml2;

  /// Optional derivative input variable name
  const std::optional<neml2::VariableName> _neml2_input_derivative;

  /// Optional parameter derivative name
  const std::optional<std::string> _neml2_parameter_derivative;

  enum class ValueKind
  {
    Output,
    OutputDerivative,
    ParameterDerivative
  };

  const ValueKind _value_kind;
#endif
};

#define DefineNEML2ToMOOSESideMaterialPropertyAlias(T, alias)                                      \
  using NEML2ToMOOSESide##alias##MaterialProperty = NEML2ToMOOSESideMaterialProperty<T>

DefineNEML2ToMOOSESideMaterialPropertyAlias(Real, Real);
DefineNEML2ToMOOSESideMaterialPropertyAlias(SymmetricRankTwoTensor, SymmetricRankTwoTensor);
DefineNEML2ToMOOSESideMaterialPropertyAlias(SymmetricRankFourTensor, SymmetricRankFourTensor);
DefineNEML2ToMOOSESideMaterialPropertyAlias(RealVectorValue, RealVectorValue);
DefineNEML2ToMOOSESideMaterialPropertyAlias(RankTwoTensor, RankTwoTensor);
DefineNEML2ToMOOSESideMaterialPropertyAlias(RankFourTensor, RankFourTensor);
