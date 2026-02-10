//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NEML2ToMOOSEMaterialProperty.h"
#include "NEML2ModelExecutor.h"

#define registerNEML2ToMOOSEMaterialProperty(alias)                                                \
  registerMooseObject("MooseApp", NEML2ToMOOSE##alias##MaterialProperty)

registerNEML2ToMOOSEMaterialProperty(Real);
registerNEML2ToMOOSEMaterialProperty(SymmetricRankTwoTensor);
registerNEML2ToMOOSEMaterialProperty(SymmetricRankFourTensor);
registerNEML2ToMOOSEMaterialProperty(RealVectorValue);
registerNEML2ToMOOSEMaterialProperty(RankTwoTensor);
registerNEML2ToMOOSEMaterialProperty(RankFourTensor);

template <typename T>
InputParameters
NEML2ToMOOSEMaterialProperty<T>::validParams()
{
  auto params = Material::validParams();
  params.addClassDescription("Provide an output (or its derivative) from a NEML2 model as a MOOSE "
                             "material property of type " +
                             demangle(typeid(T).name()) + ".");

  params.addRequiredParam<UserObjectName>("neml2_executor",
                                          "User object managing the execution of the NEML2 model.");
  params.addRequiredParam<MaterialPropertyName>(
      "to_moose",
      "MOOSE material property used to store the NEML2 output variable (or its derivative)");
  params.addRequiredParam<std::string>("from_neml2", "NEML2 output variable to read from");
  params.addParam<std::string>(
      "neml2_input_derivative",

      "If supplied return the derivative of the NEML2 output variable with respect to this");
  params.addParam<std::string>(
      "neml2_parameter_derivative",
      "If supplied return the derivative of neml2_variable with respect to this");

  // provide an optional initialization of the moose property (because we don't really know if it is
  // going to become stateful or not)
  params.addParam<MaterialPropertyName>("moose_material_property_init",
                                        "Optional material property as the initial condition");

  return params;
}

template <typename T>
NEML2ToMOOSEMaterialProperty<T>::NEML2ToMOOSEMaterialProperty(const InputParameters & params)
  : Material(params)
#ifdef NEML2_ENABLED
    ,
    _execute_neml2_model(getUserObject<NEML2ModelExecutor>("neml2_executor")),
    _prop(declareProperty<T>(getParam<MaterialPropertyName>("to_moose"))),
    _prop0(isParamValid("moose_material_property_init")
               ? &getMaterialProperty<T>("moose_material_property_init")
               : nullptr),
    _value(
        !isParamValid("neml2_input_derivative")
            ? (!isParamValid("neml2_parameter_derivative")
                   ? _execute_neml2_model.getOutput(
                         NEML2Utils::parseVariableName(getParam<std::string>("from_neml2")))
                   : _execute_neml2_model.getOutputParameterDerivative(
                         NEML2Utils::parseVariableName(getParam<std::string>("from_neml2")),
                         getParam<std::string>("neml2_parameter_derivative")))
            : _execute_neml2_model.getOutputDerivative(
                  NEML2Utils::parseVariableName(getParam<std::string>("from_neml2")),
                  NEML2Utils::parseVariableName(getParam<std::string>("neml2_input_derivative")))),
    _from_neml2(NEML2Utils::parseVariableName(getParam<std::string>("from_neml2"))),
    _neml2_input_derivative(isParamValid("neml2_input_derivative")
                                ? std::make_optional(NEML2Utils::parseVariableName(
                                      getParam<std::string>("neml2_input_derivative")))
                                : std::nullopt),
    _neml2_parameter_derivative(
        isParamValid("neml2_parameter_derivative")
            ? std::make_optional(getParam<std::string>("neml2_parameter_derivative"))
            : std::nullopt),
    _value_kind(
        _neml2_input_derivative
            ? ValueKind::OutputDerivative
            : (_neml2_parameter_derivative ? ValueKind::ParameterDerivative : ValueKind::Output))
#endif
{
  NEML2Utils::assertNEML2Enabled();
}

#ifdef NEML2_ENABLED
template <typename T>
void
NEML2ToMOOSEMaterialProperty<T>::computeProperties()
{
  // See issue #28971: Using _prop0 to set initial condition for this possibly stateful property may
  // not work. As a workaround, we set the initial condition here when _t_step == 0.
  if (_t_step == 0 && _prop0)
  {
    _prop.set() = _prop0->get();
    return;
  }

  if (!_execute_neml2_model.outputReady())
    return;

  // NEML2 outputs are defined on element volume quadrature points.
  // If we're in a boundary/neighbor material context, skip to avoid size mismatch.
  if (_bnd || _neighbor)
    return;

  // look up start index for current element (local or ghost cache)
  const auto info = _execute_neml2_model.getBatchInfo(_current_elem->id());
  const bool use_global = !info.local;

  const neml2::Tensor * src = nullptr;
  if (_value_kind == ValueKind::Output)
    src = &_execute_neml2_model.getOutputTensor(_from_neml2, use_global);
  else if (_value_kind == ValueKind::OutputDerivative)
    src = &_execute_neml2_model.getOutputDerivativeTensor(
        _from_neml2, *_neml2_input_derivative, use_global);
  else
    src = &_execute_neml2_model.getOutputParameterDerivativeTensor(
        _from_neml2, *_neml2_parameter_derivative, use_global);

  const auto nqp = info.nqp;
  const auto prop_size = _prop.size();

  if (prop_size != nqp)
    mooseError("NEML2ToMOOSEMaterialProperty: size mismatch for '",
               _from_neml2,
               "': NEML2 nqp=",
               nqp,
               ", property size=",
               prop_size,
               ", elem=",
               _current_elem->id(),
               ", is boundary=",
               _bnd,
               ", is neighbor=",
               _neighbor,
               ". Skipping.");
  for (_qp = 0; _qp < nqp; ++_qp)
    NEML2Utils::copyTensorToMOOSEData(src->batch_index({neml2::Size(info.start + _qp)}),
                                      _prop[_qp]);
}
#endif

#define instantiateNEML2ToMOOSEMaterialProperty(T) template class NEML2ToMOOSEMaterialProperty<T>

instantiateNEML2ToMOOSEMaterialProperty(Real);
instantiateNEML2ToMOOSEMaterialProperty(SymmetricRankTwoTensor);
instantiateNEML2ToMOOSEMaterialProperty(SymmetricRankFourTensor);
instantiateNEML2ToMOOSEMaterialProperty(RealVectorValue);
instantiateNEML2ToMOOSEMaterialProperty(RankTwoTensor);
instantiateNEML2ToMOOSEMaterialProperty(RankFourTensor);
