//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "CZMRealVectorCartesianComponent.h"

registerMooseObject("SolidMechanicsApp", CZMRealVectorCartesianComponent);
registerMooseObject("SolidMechanicsApp", ADCZMRealVectorCartesianComponent);

template <bool is_ad>
InputParameters
CZMRealVectorCartesianComponentTempl<is_ad>::validParams()
{
  InputParameters params = InterfaceMaterial::validParams();
  params.addClassDescription("Access a component of a RealVectorValue defined on a cohesive zone");
  params.addRequiredParam<std::string>("real_vector_value", "The vector material name");
  params.addRequiredParam<MaterialPropertyName>(
      "property_name", "Name of the material property computed by this model");
  params.addRequiredRangeCheckedParam<unsigned int>(
      "index", "index >= 0 & index <= 2", "The vector component output (0, 1, 2)");
  params.addParam<std::string>("base_name", "Material property base name");
  return params;
}

template <bool is_ad>
CZMRealVectorCartesianComponentTempl<is_ad>::CZMRealVectorCartesianComponentTempl(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _base_name(isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                   ? getParam<std::string>("base_name") + "_"
                   : ""),
    _property(this->template declareGenericPropertyByName<Real, is_ad>(
        getParam<MaterialPropertyName>("property_name"))),
    _vector(this->template getGenericMaterialPropertyByName<RealVectorValue, is_ad>(
        _base_name + getParam<std::string>("real_vector_value"))),
    _index(getParam<unsigned int>("index"))
{
}

template <bool is_ad>
void
CZMRealVectorCartesianComponentTempl<is_ad>::computeQpProperties()
{
  _property[_qp] = _vector[_qp](_index);
}

template class CZMRealVectorCartesianComponentTempl<false>;
template class CZMRealVectorCartesianComponentTempl<true>;
