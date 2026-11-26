//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosBiLinearMixedModeTraction.h"
#include "KokkosSCZMStabilizationData.h"
#include "FEProblemBase.h"

#include <algorithm>
#include <sstream>

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp", KokkosBiLinearMixedModeTraction);

namespace
{
/// Prefixes a property name with base_name; a number names a constant property and stays as is
std::string
prefixedPropertyOrNumber(const std::string & prefix, const MaterialPropertyName & prop_name)
{
  std::istringstream ss(prop_name);
  Real value;
  if (ss >> value && ss.eof())
    return prop_name;
  return prefix + prop_name;
}
}

InputParameters
KokkosBiLinearMixedModeTraction::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addRequiredParam<Real>("penalty_stiffness", "Penalty stiffness.");
  params.addRequiredParam<MaterialPropertyName>(
      "GI_c", "Critical energy release rate in normal direction.");
  params.addRequiredParam<MaterialPropertyName>("GII_c",
                                                "Critical energy release rate in shear direction.");
  params.addRequiredParam<MaterialPropertyName>("normal_strength",
                                                "Tensile strength in normal direction.");
  params.addRequiredParam<MaterialPropertyName>("shear_strength",
                                                "Tensile strength in shear direction.");
  params.addRequiredParam<Real>("eta", "The power law parameter.");
  params.addParam<Real>("viscosity", 0.0, "Viscosity.");
  MooseEnum criterion("POWER_LAW BK", "BK");
  params.addParam<MooseEnum>(
      "mixed_mode_criterion", criterion, "Option for mixed mode propagation criterion.");
  params.addParam<bool>(
      "lag_mode_mixity", true, "Whether to use old displacement jumps to compute the mode mixity.");
  params.addParam<bool>(
      "lag_displacement_jump",
      false,
      "Whether to use old displacement jumps to compute the effective displacement jump.");
  params.addParam<Real>("alpha", 1e-10, "Regularization parameter for the Macaulay bracket.");
  params.addParamNamesToGroup("lag_mode_mixity lag_displacement_jump alpha", "Advanced");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addClassDescription(
      "Computes a mixed mode bilinear cohesive traction-separation law with Kokkos.");
  return params;
}

KokkosBiLinearMixedModeTraction::KokkosBiLinearMixedModeTraction(const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ndisp(_mesh.dimension()),
    _penalty_stiffness(getParam<Real>("penalty_stiffness")),
    _eta(getParam<Real>("eta")),
    _viscosity(getParam<Real>("viscosity")),
    _criterion(
        getParam<MooseEnum>("mixed_mode_criterion")
            .getEnum<Moose::Kokkos::SCZM::BiLinearMixedModeCohesiveLaw::MixedModeCriterion>()),
    _lag_mode_mixity(getParam<bool>("lag_mode_mixity")),
    _lag_displacement_jump(getParam<bool>("lag_displacement_jump")),
    _alpha(getParam<Real>("alpha"))
{
  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";

  _damage = declareKokkosPropertyByName<Real>(prefix + "damage");
  _damage_old = getKokkosMaterialPropertyOldByName<Real>(prefix + "damage");
  _history_jump = declareKokkosPropertyByName<Real, 1>(prefix + "damage_history_jump", {_ndisp});
  _history_jump_old = getKokkosMaterialPropertyOldByName<Real, 1>(prefix + "damage_history_jump");
  _interface_displacement_jump =
      getKokkosMaterialPropertyByName<Real, 1>(prefix + "interface_displacement_jump");

  _GI_c = getKokkosMaterialPropertyByName<Real>(
      prefixedPropertyOrNumber(prefix, getParam<MaterialPropertyName>("GI_c")));
  _GII_c = getKokkosMaterialPropertyByName<Real>(
      prefixedPropertyOrNumber(prefix, getParam<MaterialPropertyName>("GII_c")));
  _normal_strength = getKokkosMaterialPropertyByName<Real>(
      prefixedPropertyOrNumber(prefix, getParam<MaterialPropertyName>("normal_strength")));
  _shear_strength = getKokkosMaterialPropertyByName<Real>(
      prefixedPropertyOrNumber(prefix, getParam<MaterialPropertyName>("shear_strength")));

  _interface_traction =
      declareKokkosPropertyByName<Real, 1>(prefix + "interface_traction", {_ndisp});
  _dinterface_traction_djump =
      declareKokkosPropertyByName<Real, 2>(prefix + "dinterface_traction_djump", {_ndisp, _ndisp});
}

void
KokkosBiLinearMixedModeTraction::initialSetup()
{
  Moose::Kokkos::InterfaceMaterial::initialSetup();

  TheWarehouse::Query uo_query =
      _fe_problem.theWarehouse().query().condition<AttribSystem>("KokkosUserObject");
  std::vector<UserObjectBase *> userobjs;
  uo_query.queryInto(userobjs);

  const KokkosSCZMStabilizationData * matched_uo = nullptr;
  for (const auto * const obj : userobjs)
  {
    const auto * const sczm_uo = dynamic_cast<const KokkosSCZMStabilizationData *>(obj);
    if (!sczm_uo)
      continue;
    const auto & uo_boundaries = sczm_uo->boundaryIDs();
    if (std::none_of(boundaryIDs().begin(),
                     boundaryIDs().end(),
                     [&](const auto id) { return uo_boundaries.count(id); }))
      continue;
    if (matched_uo && matched_uo != sczm_uo)
      mooseError("KokkosBiLinearMixedModeTraction '",
                 name(),
                 "' found multiple KokkosSCZMStabilizationData user objects overlapping its "
                 "boundaries.");
    matched_uo = sczm_uo;
  }

  _stabilized = matched_uo != nullptr;
}

void
KokkosBiLinearMixedModeTraction::robinPrepass(const KokkosSCZMStabilizationData & uo) const
{
  uo.robinPrepass(*this);
}
