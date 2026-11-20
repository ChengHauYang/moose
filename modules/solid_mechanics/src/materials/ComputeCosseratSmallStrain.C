//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeCosseratSmallStrain.h"

// MOOSE includes
#include "PermutationTensor.h"

#include "libmesh/quadrature.h"

registerMooseObject("SolidMechanicsApp", ComputeCosseratSmallStrain);
registerMooseObject("SolidMechanicsApp", ADComputeCosseratSmallStrain);

template <bool is_ad>
InputParameters
ComputeCosseratSmallStrainTempl<is_ad>::validParams()
{
  InputParameters params = StrainBase::validParams();
  params.addClassDescription("Compute small Cosserat strains");
  params.addRequiredCoupledVar("Cosserat_rotations", "The 3 Cosserat rotation variables");
  return params;
}

template <bool is_ad>
ComputeCosseratSmallStrainTempl<is_ad>::ComputeCosseratSmallStrainTempl(
    const InputParameters & parameters)
  : StrainBase(parameters),
    _curvature(this->template declareGenericProperty<RankTwoTensor, is_ad>("curvature")),
    _nrots(this->coupledComponents("Cosserat_rotations")),
    _wc(this->template coupledGenericValues<is_ad>("Cosserat_rotations")),
    _grad_wc(this->template coupledGenericGradients<is_ad>("Cosserat_rotations"))
{
  if (_nrots != 3)
    mooseError("ComputeCosseratSmallStrain: This Material is only defined for 3-dimensional "
               "simulations so 3 Cosserat rotation variables are needed");
}

template <bool is_ad>
void
ComputeCosseratSmallStrainTempl<is_ad>::computeQpProperties()
{
  auto strain = GenericRankTwoTensor<is_ad>::initializeFromRows(
      (*_grad_disp[0])[_qp], (*_grad_disp[1])[_qp], (*_grad_disp[2])[_qp]);
  GenericRealVectorValue<is_ad> wc_vector((*_wc[0])[_qp], (*_wc[1])[_qp], (*_wc[2])[_qp]);

  for (const auto i : make_range(Moose::dim))
    for (const auto j : make_range(Moose::dim))
      for (const auto k : make_range(Moose::dim))
        strain(i, j) += PermutationTensor::eps(i, j, k) * wc_vector(k);

  _total_strain[_qp] = strain;

  _mechanical_strain[_qp] = strain;
  for (const auto es : _eigenstrains)
    _mechanical_strain[_qp] -= (*es)[_qp];

  _curvature[_qp] = GenericRankTwoTensor<is_ad>::initializeFromRows(
      (*_grad_wc[0])[_qp], (*_grad_wc[1])[_qp], (*_grad_wc[2])[_qp]);
}

template class ComputeCosseratSmallStrainTempl<false>;
template class ComputeCosseratSmallStrainTempl<true>;
