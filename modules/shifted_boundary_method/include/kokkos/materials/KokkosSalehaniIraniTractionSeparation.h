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
#include "SCZMSalehaniIraniCohesiveLaw.h"
#include "SCZMTrialTractionProvider.h"

/// Computes the Salehani-Irani 3DC local cohesive traction-separation law with Kokkos.
class KokkosSalehaniIraniTractionSeparation : public Moose::Kokkos::InterfaceMaterial,
                                              public SCZMTrialTractionProvider
{
public:
  static InputParameters validParams();

  KokkosSalehaniIraniTractionSeparation(const InputParameters & parameters);

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

    const auto evaluation = cohesiveLaw().evaluate(jump, _ndisp);
    auto traction = _interface_traction(elem_datum, qp);
    auto tangent = _dinterface_traction_djump(elem_datum, qp);
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      traction(i) = evaluation.traction(i);
      for (unsigned int j = 0; j < _ndisp; ++j)
        tangent(i, j) = evaluation.jump_tangent(i, j);
    }
  }

  KOKKOS_FUNCTION Moose::Kokkos::SCZM::SalehaniIraniCohesiveLaw cohesiveLaw() const
  {
    Moose::Kokkos::SCZM::SalehaniIraniCohesiveLaw law;
    law.normal_gap_at_maximum_normal_traction = _normal_gap_at_maximum_normal_traction;
    law.tangential_gap_at_maximum_shear_traction = _tangential_gap_at_maximum_shear_traction;
    law.maximum_normal_traction = _maximum_normal_traction;
    law.maximum_shear_traction = _maximum_shear_traction;
    return law;
  }

  /// The law is memoryless, so the trial law is the law itself at every QP
  template <typename DatumType>
  KOKKOS_FUNCTION Moose::Kokkos::SCZM::SalehaniIraniCohesiveLaw trialLaw(DatumType &,
                                                                         unsigned int) const
  {
    return cohesiveLaw();
  }

  void robinPrepass(const KokkosSCZMStabilizationData & uo) const override;

private:
  const unsigned int _ndisp;
  const Real _normal_gap_at_maximum_normal_traction;
  const Real _tangential_gap_at_maximum_shear_traction;
  const Real _maximum_normal_traction;
  const Real _maximum_shear_traction;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_displacement_jump;
  Moose::Kokkos::MaterialProperty<Real, 1> _interface_traction;
  Moose::Kokkos::MaterialProperty<Real, 2> _dinterface_traction_djump;
};
