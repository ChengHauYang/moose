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

#ifdef NEML2_ENABLED
namespace
{
const neml2::Tensor &
requestedNEML2Value(const InputParameters & params, const NEML2ModelExecutor & executor)
{
  const auto output_name = NEML2Utils::parseVariableName(params.get<std::string>("from_neml2"));

  if (params.isParamValid("neml2_input_derivative"))
    return executor.getOutputDerivative(
        output_name,
        NEML2Utils::parseVariableName(params.get<std::string>("neml2_input_derivative")));

  if (params.isParamValid("neml2_parameter_derivative"))
    return executor.getOutputParameterDerivative(
        output_name, params.get<std::string>("neml2_parameter_derivative"));

  return executor.getOutput(output_name);
}
}
#endif

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
  params.addParam<UserObjectName>(
      "neml2_executor_side", "User object managing the execution of the NEML2 model on sides.");

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
    _execute_neml2_model_side(isParamValid("neml2_executor_side") &&
                                      hasUserObject<NEML2ModelExecutor>("neml2_executor_side")
                                  ? &getUserObject<NEML2ModelExecutor>("neml2_executor_side")
                                  : nullptr),
    _prop(declareProperty<T>(getParam<MaterialPropertyName>("to_moose"))),
    _prop0(isParamValid("moose_material_property_init")
               ? &getMaterialProperty<T>("moose_material_property_init")
               : nullptr),
    _value(requestedNEML2Value(params, _execute_neml2_model)),
    _value_side(_execute_neml2_model_side ? requestedNEML2Value(params, *_execute_neml2_model_side)
                                          : _value)
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

  if (_bnd && !_execute_neml2_model_side)
    mooseError("Boundary NEML2 material retrieval requires 'neml2_executor_side'.");

  const auto & executor = _bnd ? *_execute_neml2_model_side : _execute_neml2_model;
  const neml2::Tensor * value = _bnd ? &_value_side : &_value;

  if (!executor.outputReady())
    return;

#ifdef DEBUG
  // std::cout << "_execute_neml2_model.boundaryRestricted(): "
  //           << _execute_neml2_model.boundaryRestricted() << std::endl;

  // std::cout << "_qrule->n_points() = " << _qrule->n_points() << std::endl;

  // std::cout << "_material_data_type: ";
  // switch (_material_data_type)
  // {
  //   case Moose::BLOCK_MATERIAL_DATA:
  //     std::cout << "BLOCK_MATERIAL_DATA";
  //     break;
  //   case Moose::BOUNDARY_MATERIAL_DATA:
  //     std::cout << "BOUNDARY_MATERIAL_DATA";
  //     break;
  //   case Moose::FACE_MATERIAL_DATA:
  //     std::cout << "FACE_MATERIAL_DATA";
  //     break;
  //   case Moose::NEIGHBOR_MATERIAL_DATA:
  //     std::cout << "NEIGHBOR_MATERIAL_DATA";
  //     break;
  //   case Moose::INTERFACE_MATERIAL_DATA:
  //     std::cout << "INTERFACE_MATERIAL_DATA";
  //     break;
  //   default:
  //     std::cout << "UNKNOWN_MATERIAL_DATA_TYPE";
  // }
  // std::cout << ", _bnd = " << _bnd << ", ";
  // for (_qp = 0; _qp < _qrule->n_points(); ++_qp)
  //   std::cout << "q_point[" << _qp << "] = " << _q_point[_qp] << std::endl;

#endif

  // if this material is face but the side batch index for the current element side
  // does not exist, return without doing anything
  std::size_t i = 0;
#ifdef DEBUG
  std::size_t i_volume = 0;
  bool compare_volume_and_side = false;
#endif
  if (_bnd)
  {
    const NEML2BatchIndexGenerator::ElemSide elem_side{_current_elem->id(), _current_side};
    if (!executor.isSideBatchIndexExist(elem_side))
    {
      // fall back to element batch index if side batch index does not exist
      i = _execute_neml2_model.getBatchIndex(_current_elem->id());
      value = &_value;
    }
    else
    {
      i = executor.getSideBatchIndex(elem_side);
#ifdef DEBUG
      if (!_execute_neml2_model.boundaryRestricted())
      {
        i_volume = _execute_neml2_model.getBatchIndex(_current_elem->id());
        compare_volume_and_side = true;
      }
#endif
    }
  }
  else
    i = executor.getBatchIndex(_current_elem->id());

#ifdef DEBUG
  // if (_bnd)
  //   std::cout << "NEML2ToMOOSEMaterialProperty: boundary material data, current element ID = "
  //             << _current_elem->id() << ", current side = " << _current_side << std::endl;

  if (compare_volume_and_side)
  {
    std::cout << "------------\n";
    for (std::size_t qp = 0; qp < _qrule->n_points(); ++qp)
    {
      const auto value_volume = _value.batch_index({neml2::Size(i_volume + qp)});
      const auto value_side = _value_side.batch_index({neml2::Size(i + qp)});
      const auto abs_diff = (value_volume - value_side).abs();
      const auto rel_diff = abs_diff / (value_side.abs() + 1e-16);

      std::cout << "q_point[" << qp << "] = " << _q_point[qp] << std::endl;
      std::cout << "NEML2ToMOOSEMaterialProperty: side/volume "
                << " for element " << _current_elem->id() << ", side " << _current_side << ", qp "
                << qp << std::endl
                << "  abs(value_volume - value_side) / (abs(value_side) + 1e-16) = " << rel_diff
                << std::endl;
    }
  }
#endif

  // look up start index for current element
  for (_qp = 0; _qp < _qrule->n_points(); ++_qp)
    NEML2Utils::copyTensorToMOOSEData(value->batch_index({neml2::Size(i + _qp)}), _prop[_qp]);
}
#endif

#define instantiateNEML2ToMOOSEMaterialProperty(T) template class NEML2ToMOOSEMaterialProperty<T>

instantiateNEML2ToMOOSEMaterialProperty(Real);
instantiateNEML2ToMOOSEMaterialProperty(SymmetricRankTwoTensor);
instantiateNEML2ToMOOSEMaterialProperty(SymmetricRankFourTensor);
instantiateNEML2ToMOOSEMaterialProperty(RealVectorValue);
instantiateNEML2ToMOOSEMaterialProperty(RankTwoTensor);
instantiateNEML2ToMOOSEMaterialProperty(RankFourTensor);
