//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "CompositeElasticityTensor.h"

registerMooseObject("SolidMechanicsApp", CompositeElasticityTensor);
registerMooseObject("SolidMechanicsApp", ADCompositeElasticityTensor);

template <bool is_ad>
InputParameters
CompositeElasticityTensorTempl<is_ad>::validParams()
{
  InputParameters params = CompositeTensorBase<RankFourTensor, Material, is_ad>::validParams();
  params.addClassDescription("Assemble an elasticity tensor from multiple tensor contributions "
                             "weighted by material properties");
  params.addParam<std::string>("base_name",
                               "Optional parameter that allows the user to define "
                               "multiple mechanics material systems on the same "
                               "block, i.e. for multiple phases");
  return params;
}

template <bool is_ad>
CompositeElasticityTensorTempl<is_ad>::CompositeElasticityTensorTempl(
    const InputParameters & parameters)
  : CompositeTensorBase<RankFourTensor, Material, is_ad>(parameters),
    _base_name(this->isParamValid("base_name")
                   ? this->template getParam<std::string>("base_name") + "_"
                   : ""),
    _M_name(_base_name + "elasticity_tensor"),
    _M(this->template declareGenericProperty<RankFourTensor, is_ad>(_M_name))
{
  // we take the tensor names to be the _base names_ of the elasticity tensors
  for (const auto i : make_range(_num_comp))
    _tensor_names[i] += "_elasticity_tensor";

  initializeDerivativeProperties(_M_name);
}

template <bool is_ad>
void
CompositeElasticityTensorTempl<is_ad>::computeQpProperties()
{
  computeQpTensorProperties(_M);
}

template class CompositeElasticityTensorTempl<false>;
template class CompositeElasticityTensorTempl<true>;
