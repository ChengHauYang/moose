//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "MOOSESideMaterialPropertyToNEML2.h"

#define registerMOOSESideMaterialPropertyToNEML2(alias)                                            \
  registerMooseObject("MooseApp", MOOSESide##alias##MaterialPropertyToNEML2);                      \
  registerMooseObject("MooseApp", MOOSEOldSide##alias##MaterialPropertyToNEML2)

registerMOOSESideMaterialPropertyToNEML2(Real);
registerMOOSESideMaterialPropertyToNEML2(RankTwoTensor);
registerMOOSESideMaterialPropertyToNEML2(SymmetricRankTwoTensor);
registerMOOSESideMaterialPropertyToNEML2(RealVectorValue);

template <typename T, unsigned int state>
InputParameters
MOOSESideMaterialPropertyToNEML2<T, state>::validParams()
{
  auto params = MOOSEToNEML2SideBatched<T>::validParams();
  params.addClassDescription("Gather a MOOSE material property on sidesets for insertion into the "
                             "specified input or model parameter of a NEML2 model.");
  params.template addRequiredParam<MaterialPropertyName>("from_moose",
                                                         "MOOSE material property to read from");
  return params;
}

template <typename T, unsigned int state>
MOOSESideMaterialPropertyToNEML2<T, state>::MOOSESideMaterialPropertyToNEML2(
    const InputParameters & params)
  : MOOSEToNEML2SideBatched<T>(params) // MOOSEToNEML2SideBatched is inherited from SideUserObject
#ifdef NEML2_ENABLED
    ,
    _mat_prop(this->template getGenericMaterialProperty<T, false>("from_moose", state))
#endif
{
}

template class MOOSESideMaterialPropertyToNEML2<Real, 0>;
template class MOOSESideMaterialPropertyToNEML2<Real, 1>;
template class MOOSESideMaterialPropertyToNEML2<RankTwoTensor, 0>;
template class MOOSESideMaterialPropertyToNEML2<RankTwoTensor, 1>;
template class MOOSESideMaterialPropertyToNEML2<SymmetricRankTwoTensor, 0>;
template class MOOSESideMaterialPropertyToNEML2<SymmetricRankTwoTensor, 1>;
template class MOOSESideMaterialPropertyToNEML2<RealVectorValue, 0>;
template class MOOSESideMaterialPropertyToNEML2<RealVectorValue, 1>;
