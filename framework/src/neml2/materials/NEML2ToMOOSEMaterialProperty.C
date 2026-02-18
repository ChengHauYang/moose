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
                  NEML2Utils::parseVariableName(getParam<std::string>("neml2_input_derivative"))))
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

#ifdef DEBUG
  // if (!_execute_neml2_model.hasBatchIndex(_current_elem->id()))
  // {
  //   _console << "_material_data_type: ";
  //   switch (_material_data_type)
  //   {
  //     case Moose::BLOCK_MATERIAL_DATA:
  //       _console << "BLOCK_MATERIAL_DATA";
  //       break;
  //     case Moose::BOUNDARY_MATERIAL_DATA:
  //       _console << "BOUNDARY_MATERIAL_DATA";
  //       break;
  //     case Moose::FACE_MATERIAL_DATA:
  //       _console << "FACE_MATERIAL_DATA";
  //       break;
  //     case Moose::NEIGHBOR_MATERIAL_DATA:
  //       _console << "NEIGHBOR_MATERIAL_DATA";
  //       break;
  //     case Moose::INTERFACE_MATERIAL_DATA:
  //       _console << "INTERFACE_MATERIAL_DATA";
  //       break;
  //     default:
  //       _console << "UNKNOWN_MATERIAL_DATA_TYPE";
  //   }
  //   _console << "_bnd = " << _bnd << std::endl;
  //   _console << std::endl;
  // }

  _console << "_material_data_type: ";
  switch (_material_data_type)
  {
    case Moose::BLOCK_MATERIAL_DATA:
      _console << "BLOCK_MATERIAL_DATA";
      break;
    case Moose::BOUNDARY_MATERIAL_DATA:
      _console << "BOUNDARY_MATERIAL_DATA";
      break;
    case Moose::FACE_MATERIAL_DATA:
      _console << "FACE_MATERIAL_DATA";
      break;
    case Moose::NEIGHBOR_MATERIAL_DATA:
      _console << "NEIGHBOR_MATERIAL_DATA";
      break;
    case Moose::INTERFACE_MATERIAL_DATA:
      _console << "INTERFACE_MATERIAL_DATA";
      break;
    default:
      _console << "UNKNOWN_MATERIAL_DATA_TYPE";
  }
  _console << ", _bnd = " << _bnd;

  _console << ", proc = " << processor_id() << ", elem=" << _current_elem->id() << std::endl;

  for (int side = 0; side < _current_elem->n_sides(); ++side)
    if (_execute_neml2_model.hasSideBatchIndex(NEML2SideBatchIndexGenerator::ElemSide{0, side}))
      _console << "elem 0 side " << side << " has side batch index" << std::endl;
    else
      _console << "elem 0 side " << side << " does NOT have side batch index" << std::endl;

  for (int side = 0; side < _current_elem->n_sides(); ++side)
    if (_execute_neml2_model.hasSideBatchIndex(NEML2SideBatchIndexGenerator::ElemSide{1, side}))
      _console << "elem 1 side " << side << " has side batch index" << std::endl;
    else
      _console << "elem 1 side " << side << " does NOT have side batch index" << std::endl;

  _console << "elem id = " << _current_elem->id() << " ,";
  _console << "elem side = " << _current_side << " ,";
  _console << " _qrule->n_points() = " << _qrule->n_points() << " ,";
  for (_qp = 0; _qp < _qrule->n_points(); ++_qp)
    _console << "pos = " << _q_point[_qp];

#endif
  // TODO: some NEIGHBOR_MATERIAL_DATA errors come when we go to mpirun, and cannot find the batch
  // index. Still do not know how to deal with this.

  // this is dangerous but I just want to do some easy tests first!!!
  // TODO: remove this

  // if (_material_data_type == Moose::NEIGHBOR_MATERIAL_DATA && _bnd)
  //   return;

  // Side batches are currently generated from boundary side loops. Therefore we only use side
  // indices for boundary-restricted materials and fall back to volume indices for other material
  // data contexts.
  const bool use_side = _bnd;

  // std::ofstream fout("side_pos.csv", std::ios::app);
  // if (_material_data_type != Moose::BLOCK_MATERIAL_DATA)
  // {
  //   const auto pos = _current_elem->vertex_average();
  //   fout << pos(0) << ", " << pos(1) << ", " << pos(2);
  //   fout << std::endl;
  // }

  // fout.close();

  // std::ofstream fout1("bnd.csv", std::ios::app);
  // if (_bnd)
  // {
  //   const auto pos = _current_elem->vertex_average();
  //   fout1 << pos(0) << ", " << pos(1) << ", " << pos(2);
  //   fout1 << std::endl;
  // }

  // fout1.close();

#ifdef DEBUG
  std::ofstream fout2("q_point.csv", std::ios::app);
#endif

  const auto start =
      use_side ? _execute_neml2_model.getSideBatchIndex(
                     NEML2SideBatchIndexGenerator::ElemSide{_current_elem->id(), _current_side})
               : _execute_neml2_model.getBatchIndex(_current_elem->id());

  for (_qp = 0; _qp < _qrule->n_points(); ++_qp)
  {
#ifdef DEBUG
    const auto pos = _q_point[_qp];
    fout2 << pos(0) << ", " << pos(1) << ", " << pos(2);
    fout2 << std::endl;

    _console << "proc = " << processor_id() << " elem=" << _current_elem->id() << " qp=" << _qp
             << ", x=" << _q_point[_qp];
#endif

    NEML2Utils::copyTensorToMOOSEData(_value.batch_index({neml2::Size(start + _qp)}), _prop[_qp]);

#ifdef DEBUG
    _console << "\nprop=" << _prop[_qp] << std::endl;
#endif
  }
#ifdef DEBUG
  fout2.close();
  _console << "----------------------";
  _console << std::endl;
#endif
}
#endif

#define instantiateNEML2ToMOOSEMaterialProperty(T) template class NEML2ToMOOSEMaterialProperty<T>

instantiateNEML2ToMOOSEMaterialProperty(Real);
instantiateNEML2ToMOOSEMaterialProperty(SymmetricRankTwoTensor);
instantiateNEML2ToMOOSEMaterialProperty(SymmetricRankFourTensor);
instantiateNEML2ToMOOSEMaterialProperty(RealVectorValue);
instantiateNEML2ToMOOSEMaterialProperty(RankTwoTensor);
instantiateNEML2ToMOOSEMaterialProperty(RankFourTensor);
