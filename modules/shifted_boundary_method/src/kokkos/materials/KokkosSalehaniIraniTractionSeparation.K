//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSalehaniIraniTractionSeparation.h"
#include "KokkosSCZMStabilizationData.h"

registerKokkosInterfaceMaterial("ShiftedBoundaryMethodApp", KokkosSalehaniIraniTractionSeparation);

InputParameters
KokkosSalehaniIraniTractionSeparation::validParams()
{
  auto params = InterfaceMaterial::validParams();
  params.addRequiredParam<Real>(
      "normal_gap_at_maximum_normal_traction",
      "The value of normal gap at which maximum normal traction is achieved.");
  params.addRequiredParam<Real>(
      "tangential_gap_at_maximum_shear_traction",
      "The value of tangential gap at which maximum shear traction is achieved.");
  params.addRequiredParam<Real>("maximum_normal_traction",
                                "The maximum normal traction the interface can sustain.");
  params.addRequiredParam<Real>("maximum_shear_traction",
                                "The maximum shear traction the interface can sustain.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addClassDescription(
      "Computes the Salehani-Irani 3DC cohesive traction-separation law with Kokkos.");
  return params;
}

KokkosSalehaniIraniTractionSeparation::KokkosSalehaniIraniTractionSeparation(
    const InputParameters & parameters)
  : InterfaceMaterial(parameters),
    _ndisp(_mesh.dimension()),
    _normal_gap_at_maximum_normal_traction(getParam<Real>("normal_gap_at_maximum_normal_traction")),
    _tangential_gap_at_maximum_shear_traction(
        getParam<Real>("tangential_gap_at_maximum_shear_traction")),
    _maximum_normal_traction(getParam<Real>("maximum_normal_traction")),
    _maximum_shear_traction(getParam<Real>("maximum_shear_traction")),
    _interface_displacement_jump(getKokkosMaterialPropertyByName<Real, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        "interface_displacement_jump"))
{
  if (_normal_gap_at_maximum_normal_traction <= 0.0)
    paramError("normal_gap_at_maximum_normal_traction", "The normal gap must be positive.");
  if (_tangential_gap_at_maximum_shear_traction <= 0.0)
    paramError("tangential_gap_at_maximum_shear_traction", "The tangential gap must be positive.");
  if (_maximum_normal_traction <= 0.0)
    paramError("maximum_normal_traction", "The maximum normal traction must be positive.");
  if (_maximum_shear_traction <= 0.0)
    paramError("maximum_shear_traction", "The maximum shear traction must be positive.");

  const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                          ? getParam<std::string>("base_name") + "_"
                          : "";
  _interface_traction =
      declareKokkosPropertyByName<Real, 1>(prefix + "interface_traction", {_ndisp});
  _dinterface_traction_djump =
      declareKokkosPropertyByName<Real, 2>(prefix + "dinterface_traction_djump", {_ndisp, _ndisp});
}

void
KokkosSalehaniIraniTractionSeparation::robinPrepass(const KokkosSCZMStabilizationData & uo) const
{
  uo.robinPrepass(*this);
}

void
KokkosSalehaniIraniTractionSeparation::checkStabilizationLaw(
    const KokkosSCZMStabilizationData & uo) const
{
  if (uo.coupledCubicStabilization())
    mooseError("The weighted traction stabilization law of '",
               uo.name(),
               "' cannot select coupled_cubic with Salehani-Irani material '",
               name(),
               "'.");
}
