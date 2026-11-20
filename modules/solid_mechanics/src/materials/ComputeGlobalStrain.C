//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeGlobalStrain.h"
#include "GlobalStrainUserObjectInterface.h"

// MOOSE includes
#include "RankTwoTensor.h"

registerMooseObject("SolidMechanicsApp", ComputeGlobalStrain);
registerMooseObject("SolidMechanicsApp", ADComputeGlobalStrain);

template <bool is_ad>
InputParameters
ComputeGlobalStrainTempl<is_ad>::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription(
      "Material for storing the global strain values from the scalar variable");
  params.addParam<std::string>("base_name",
                               "Optional parameter that allows the user to define "
                               "multiple mechanics material systems on the same "
                               "block, i.e. for multiple phases");
  params.addCoupledVar("scalar_global_strain", "Scalar variable for global strain");
  params.addCoupledVar("displacements", "The name of the displacement variables");
  params.addRequiredParam<UserObjectName>("global_strain_uo",
                                          "The name of the GlobalStrainUserObject");

  params.set<MooseEnum>("constant_on") = "SUBDOMAIN";
  return params;
}

template <bool is_ad>
ComputeGlobalStrainTempl<is_ad>::ComputeGlobalStrainTempl(const InputParameters & parameters)
  : Material(parameters),
    _base_name(isParamValid("base_name") ? getParam<std::string>("base_name") + "_" : ""),
    _scalar_global_strain(this->template coupledGenericScalarValue<is_ad>("scalar_global_strain")),
    _global_strain(
        this->template declareGenericProperty<RankTwoTensor, is_ad>(_base_name + "global_strain")),
    _pst(this->template getUserObject<GlobalStrainUserObjectInterfaceTempl<is_ad>>(
        "global_strain_uo")),
    _periodic_dir(_pst.getPeriodicDirections()),
    _dim(_mesh.dimension()),
    _ndisp(coupledComponents("displacements"))
{
}

template <bool is_ad>
void
ComputeGlobalStrainTempl<is_ad>::initQpStatefulProperties()
{
  _global_strain[_qp].zero();
}

template <bool is_ad>
void
ComputeGlobalStrainTempl<is_ad>::computeQpProperties()
{
  GenericRankTwoTensor<is_ad> & strain = _global_strain[_qp];

  switch (_scalar_global_strain.size())
  {
    case 1:
      strain.zero();
      strain(0, 0) = _scalar_global_strain[0];
      break;

    case 3:
      strain.zero();
      strain(0, 0) = _scalar_global_strain[0];
      strain(1, 1) = _scalar_global_strain[1];
      strain(0, 1) = strain(1, 0) = _scalar_global_strain[2];
      break;

    case 6:
      strain(0, 0) = _scalar_global_strain[0];
      strain(1, 1) = _scalar_global_strain[1];
      strain(2, 2) = _scalar_global_strain[2];
      strain(1, 2) = strain(2, 1) = _scalar_global_strain[3];
      strain(0, 2) = strain(2, 0) = _scalar_global_strain[4];
      strain(0, 1) = strain(1, 0) = _scalar_global_strain[5];
      break;

    default:
      mooseError("Only FIRST, THIRD, or SIXTH order scalar variable can be used to build "
                 "a RankTwoTensorTempl.");
  }

  for (unsigned int dir = 0; dir < _dim; ++dir)
    if (!_periodic_dir(dir))
      for (unsigned int var = 0; var < _ndisp; ++var)
        strain(dir, var) = 0.0;
}

template class ComputeGlobalStrainTempl<false>;
template class ComputeGlobalStrainTempl<true>;
