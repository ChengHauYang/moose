//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "CompositeEigenstrain.h"

registerMooseObject("SolidMechanicsApp", CompositeEigenstrain);
registerMooseObject("SolidMechanicsApp", ADCompositeEigenstrain);

template <bool is_ad>
InputParameters
CompositeEigenstrainTempl<is_ad>::validParams()
{
  InputParameters params =
      CompositeTensorBase<RankTwoTensor, ComputeEigenstrainBaseTempl<is_ad>, is_ad>::validParams();
  params.addClassDescription("Assemble an Eigenstrain tensor from multiple tensor contributions "
                             "weighted by material properties");
  return params;
}

template <bool is_ad>
CompositeEigenstrainTempl<is_ad>::CompositeEigenstrainTempl(const InputParameters & parameters)
  : CompositeTensorBase<RankTwoTensor, ComputeEigenstrainBaseTempl<is_ad>, is_ad>(parameters)
{
  initializeDerivativeProperties(_base_name + "elastic_strain");
}

template <bool is_ad>
void
CompositeEigenstrainTempl<is_ad>::computeQpEigenstrain()
{
  // Define Eigenstrain (and fill in the derivatives of elastic strain with a prefactor of -1)
  computeQpTensorProperties(_eigenstrain, -1.0);
}

template class CompositeEigenstrainTempl<false>;
template class CompositeEigenstrainTempl<true>;
