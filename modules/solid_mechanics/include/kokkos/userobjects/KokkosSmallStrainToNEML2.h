//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosElementUserObject.h"
#include "KokkosArray.h"
#include "KokkosVariableValue.h"
#include "MOOSEToNEML2.h"
#include "MathUtils.h"

/**
 * Computes infinitesimal strain from Kokkos displacement gradients and supplies it to NEML2.
 */
class KokkosSmallStrainToNEML2 : public Moose::Kokkos::ElementUserObject, public MOOSEToNEML2
{
public:
  static InputParameters validParams();

  KokkosSmallStrainToNEML2(const InputParameters & parameters);
  KokkosSmallStrainToNEML2(const KokkosSmallStrainToNEML2 & object);

  void initialize() override;
  void finalize() override {}

  template <typename Derived>
  KOKKOS_FUNCTION void execute(Datum & datum) const;

#ifdef NEML2_ENABLED
  at::Tensor gatheredData() const override;
#endif

private:
  const unsigned int _ndisp;
  const bool _moose_to_neml2_on_gpu;
  const Moose::Kokkos::VariableGradient _grad_displacements;
  mutable Moose::Kokkos::Array<Real> _buffer;
  Moose::Kokkos::Array<dof_id_type> _element_batch_offset;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosSmallStrainToNEML2::execute(Datum & datum) const
{
  const auto offset = 6 * _element_batch_offset[datum.elemID()];

  for (const auto qp : make_range(datum.n_qps()))
  {
    const auto row = offset + 6 * qp;
    _buffer[row] = _grad_displacements(datum, qp, 0)(0);
    _buffer[row + 1] = _ndisp > 1 ? _grad_displacements(datum, qp, 1)(1) : 0;
    _buffer[row + 2] = _ndisp > 2 ? _grad_displacements(datum, qp, 2)(2) : 0;
    _buffer[row + 3] =
        _ndisp > 2
            ? MathUtils::sqrt2 * 0.5 *
                  (_grad_displacements(datum, qp, 1)(2) + _grad_displacements(datum, qp, 2)(1))
            : 0;
    _buffer[row + 4] =
        _ndisp > 2
            ? MathUtils::sqrt2 * 0.5 *
                  (_grad_displacements(datum, qp, 0)(2) + _grad_displacements(datum, qp, 2)(0))
            : 0;
    _buffer[row + 5] =
        _ndisp > 1
            ? MathUtils::sqrt2 * 0.5 *
                  (_grad_displacements(datum, qp, 0)(1) + _grad_displacements(datum, qp, 1)(0))
            : 0;
  }
}
