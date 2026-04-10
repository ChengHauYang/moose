//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DamageScaledPK1Stress.h"

registerMooseObject("SolidMechanicsTestApp", DamageScaledPK1Stress);

InputParameters
DamageScaledPK1Stress::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription("Creates pk1_stress = (1 - damage) * P0 and "
                             "dpk1_ddamage = -P0 from an existing undamaged PK1 stress.");
  params.addRequiredCoupledVar("damage", "Damage-like variable used to scale the PK1 stress.");
  params.addParam<std::string>("base_name", "", "Base name for the output PK1 material system.");
  params.addParam<std::string>(
      "source_base_name",
      "undamaged",
      "Base name of the source material system providing the undamaged pk1_stress and "
      "pk1_jacobian.");
  params.addParam<MaterialPropertyName>(
      "dpk1_ddamage_name", "dpk1_ddamage", "Material property name for dPK1/ddamage.");
  return params;
}

DamageScaledPK1Stress::DamageScaledPK1Stress(const InputParameters & parameters)
  : Material(parameters),
    _base_name(isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                   ? getParam<std::string>("base_name") + "_"
                   : ""),
    _source_base_name(isParamValid("source_base_name") &&
                              !getParam<std::string>("source_base_name").empty()
                          ? getParam<std::string>("source_base_name") + "_"
                          : ""),
    _damage(coupledValue("damage")),
    _pk1_undamaged(getMaterialPropertyByName<RankTwoTensor>(_source_base_name + "pk1_stress")),
    _dpk1_undamaged(
        getMaterialPropertyByName<RankFourTensor>(_source_base_name + "pk1_jacobian")),
    _pk1(declareProperty<RankTwoTensor>(_base_name + "pk1_stress")),
    _dpk1(declareProperty<RankFourTensor>(_base_name + "pk1_jacobian")),
    _dpk1_ddamage(
        declareProperty<RankTwoTensor>(getParam<MaterialPropertyName>("dpk1_ddamage_name")))
{
}

void
DamageScaledPK1Stress::computeQpProperties()
{
  const Real scale = 1.0 - _damage[_qp];

  _pk1[_qp] = scale * _pk1_undamaged[_qp];
  _dpk1[_qp] = scale * _dpk1_undamaged[_qp];
  _dpk1_ddamage[_qp] = -_pk1_undamaged[_qp];
}
