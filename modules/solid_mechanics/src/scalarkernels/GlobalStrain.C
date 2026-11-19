//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GlobalStrain.h"
#include "GlobalStrainUserObjectInterface.h"

// MOOSE includes
#include "Assembly.h"
#include "MooseVariableScalar.h"
#include "SystemBase.h"
#include "RankTwoTensor.h"
#include "RankFourTensor.h"

registerMooseObject("SolidMechanicsApp", GlobalStrain);
registerMooseObject("SolidMechanicsApp", ADGlobalStrain);

template <bool is_ad>
InputParameters
GlobalStrainTempl<is_ad>::validParams()
{
  InputParameters params = Parent::validParams();
  params.addClassDescription("Scalar Kernel to solve for the global strain");
  params.addRequiredParam<UserObjectName>("global_strain_uo",
                                          "The name of the GlobalStrainUserObject");

  return params;
}

template <bool is_ad>
GlobalStrainTempl<is_ad>::GlobalStrainTempl(const InputParameters & parameters)
  : Parent(parameters),
    _pst(this->template getUserObject<GlobalStrainUserObjectInterfaceTempl<is_ad>>(
        "global_strain_uo")),
    _pst_residual(_pst.getResidual()),
    _periodic_dir(_pst.getPeriodicDirections()),
    _components(this->_var.order()),
    _dim(this->_mesh.dimension())
{
  if ((_dim == 1 && this->_var.order() != FIRST) || (_dim == 2 && this->_var.order() != THIRD) ||
      (_dim == 3 && this->_var.order() != SIXTH))
    mooseError("PerdiodicStrain ScalarKernel is only compatible with scalar variables of order "
               "FIRST in 1D, THIRD in 2D, and SIXTH in 3D. Please change the order of the scalar"
               "variable according to the mesh dimension.");
  assignComponentIndices(this->_var.order());
}

template <bool is_ad>
void
GlobalStrainTempl<is_ad>::reinit()
{
  if constexpr (is_ad)
    Parent::reinit();
}

template <bool is_ad>
void
GlobalStrainTempl<is_ad>::computeResidual()
{
  if constexpr (is_ad)
    Parent::computeResidual();
  else
  {
    this->prepareVectorTag(this->_assembly, this->_var.number());
    for (this->_i = 0; this->_i < this->_local_re.size(); ++this->_i)
      this->_local_re(this->_i) += computeQpResidual();
    this->accumulateTaggedLocalResidual();
  }
}

template <bool is_ad>
void
GlobalStrainTempl<is_ad>::computeJacobian()
{
  if constexpr (is_ad)
    Parent::computeJacobian();
  else
  {
    this->prepareMatrixTag(this->_assembly, this->_var.number(), this->_var.number());
    for (this->_i = 0; this->_i < this->_local_ke.m(); ++this->_i)
      for (this->_j = 0; this->_j < this->_local_ke.m(); ++this->_j)
        // Periodic direction check is not done for Jacobian calculations to avoid zero pivot error.
        this->_local_ke(this->_i, this->_j) += _pst.getJacobian()(_components[this->_i].first,
                                                                  _components[this->_i].second,
                                                                  _components[this->_j].first,
                                                                  _components[this->_j].second);
    this->accumulateTaggedLocalMatrix();
  }
}

template <bool is_ad>
void
GlobalStrainTempl<is_ad>::computeOffDiagJacobian(unsigned int jvar)
{
  if constexpr (is_ad)
    Parent::computeOffDiagJacobian(jvar);
}

template <bool is_ad>
GenericReal<is_ad>
GlobalStrainTempl<is_ad>::computeQpResidual()
{
  if (_periodic_dir(_components[this->_i].first) || _periodic_dir(_components[this->_i].second))
    return _pst_residual(_components[this->_i].first, _components[this->_i].second);

  return 0.0;
}

template <bool is_ad>
void
GlobalStrainTempl<is_ad>::assignComponentIndices(Order var_order)
{
  switch (var_order)
  {
    case 1:
      _components[0].first = 0;
      _components[0].second = 0;
      break;

    case 3:
      _components[0].first = 0;
      _components[0].second = 0;
      _components[1].first = 1;
      _components[1].second = 1;
      _components[2].first = 0;
      _components[2].second = 1;
      break;

    case 6:
      _components[0].first = 0;
      _components[0].second = 0;
      _components[1].first = 1;
      _components[1].second = 1;
      _components[2].first = 2;
      _components[2].second = 2;
      _components[3].first = 1;
      _components[3].second = 2;
      _components[4].first = 0;
      _components[4].second = 2;
      _components[5].first = 0;
      _components[5].second = 1;
      break;

    default:
      mooseError("PerdiodicStrain ScalarKernel is only compatible with FIRST, THIRD, and SIXTH "
                 "order scalar variables.");
  }
}

template class GlobalStrainTempl<false>;
template class GlobalStrainTempl<true>;
