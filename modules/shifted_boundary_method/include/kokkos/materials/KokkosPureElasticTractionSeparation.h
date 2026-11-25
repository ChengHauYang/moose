//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceMaterial.h"
#include "SCZMCohesiveLawEvaluators.h"

/// Computes a pure elastic local cohesive traction-separation law.
class KokkosPureElasticTractionSeparation : public Moose::Kokkos::InterfaceMaterial
{
public:
  static InputParameters validParams();

  KokkosPureElasticTractionSeparation(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord &,
                                           Datum & elem_datum,
                                           Datum &,
                                           unsigned int qp,
                                           unsigned int) const
  {
    Moose::Kokkos::Real3 jump{};
    const auto displacement_jump = _interface_displacement_jump(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
      jump(i) = displacement_jump(i);

    const auto evaluation = _coupled_cubic ? coupledCubicLaw().evaluate(jump, _ndisp)
                                           : cohesiveLaw().evaluate(jump, _ndisp);
    auto traction = _interface_traction(elem_datum, qp);
    auto tangent = _dinterface_traction_djump(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      traction(i) = evaluation.traction(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
        tangent(i, j) = evaluation.jump_tangent(i, j);
    }
  }

  KOKKOS_FUNCTION Moose::Kokkos::SCZM::AffineCohesiveLaw cohesiveLaw() const
  {
    Moose::Kokkos::SCZM::AffineCohesiveLaw law;
    law.stiffness(0, 0) = _normal_stiffness;
    for (unsigned int i = 1; i < _ndisp; ++i)
      law.stiffness(i, i) = _tangent_stiffness;
    return law;
  }

  KOKKOS_FUNCTION Moose::Kokkos::SCZM::CoupledCubicCohesiveLaw coupledCubicLaw() const
  {
    Moose::Kokkos::SCZM::CoupledCubicCohesiveLaw law;
    law.linear_stiffness = _cubic_linear_stiffness;
    law.cubic_stiffness = _cubic_stiffness;
    return law;
  }

private:
  const unsigned int _ndisp;
  const bool _coupled_cubic;
  const Real _normal_stiffness;
  const Real _tangent_stiffness;
  const Real _cubic_linear_stiffness;
  const Real _cubic_stiffness;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_displacement_jump;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_traction;
  Moose::Kokkos::MaterialProperty<Real, 2> _dinterface_traction_djump;
};
