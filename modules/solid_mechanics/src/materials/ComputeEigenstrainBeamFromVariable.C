//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeEigenstrainBeamFromVariable.h"

registerMooseObject("SolidMechanicsApp", ComputeEigenstrainBeamFromVariable);
registerMooseObject("SolidMechanicsApp", ADComputeEigenstrainBeamFromVariable);

template <bool is_ad>
InputParameters
ComputeEigenstrainBeamFromVariableTempl<is_ad>::validParams()
{
  InputParameters params = ComputeEigenstrainBeamBaseTempl<is_ad>::validParams();
  params.addClassDescription("Computes an eigenstrain from a set of variables");
  params.addCoupledVar("displacement_eigenstrain_variables",
                       "A list of variable names describing the "
                       "displacement eigenstrain. If provided, there must be 3 "
                       "entries, corresponding to the axial and shear "
                       "eigenstrains in the global coordinate system.");
  params.addCoupledVar("rotational_eigenstrain_variables",
                       "A list of variable names describing the rotational "
                       "eigenstrain. If provided, there must be 3 entries, "
                       "corresponding to the rotational eigenstrain in the "
                       "global coordinate system.");
  return params;
}

template <bool is_ad>
ComputeEigenstrainBeamFromVariableTempl<is_ad>::ComputeEigenstrainBeamFromVariableTempl(
    const InputParameters & parameters)
  : ComputeEigenstrainBeamBaseTempl<is_ad>(parameters),
    _ndisp(this->coupledComponents("displacement_eigenstrain_variables")),
    _nrot(this->coupledComponents("rotational_eigenstrain_variables")),
    _disp(_ndisp > 0
              ? this->template coupledGenericValues<is_ad>("displacement_eigenstrain_variables")
              : std::vector<const GenericVariableValue<is_ad> *>(
                    3, &this->template genericZeroValue<is_ad>())),
    _rot(_nrot > 0 ? this->template coupledGenericValues<is_ad>("rotational_eigenstrain_variables")
                   : std::vector<const GenericVariableValue<is_ad> *>(
                         3, &this->template genericZeroValue<is_ad>()))
{
  if ((_ndisp != 3 && _ndisp != 0) || (_nrot != 3 && _nrot != 0))
    this->mooseError(
        "ComputeEigenstrainBeamFromVariable: If the displacement or rotational eigenstrains "
        "are provided, it should contain 3 variables corresponding to the three "
        "components in the global coordinate system.");
}

template <bool is_ad>
void
ComputeEigenstrainBeamFromVariableTempl<is_ad>::computeQpEigenstrain()
{
  for (unsigned int i = 0; i < 3; ++i)
  {
    _disp_eigenstrain[_qp](i) = (*_disp[i])[_qp];
    _rot_eigenstrain[_qp](i) = (*_rot[i])[_qp];
  }
}

template class ComputeEigenstrainBeamFromVariableTempl<false>;
template class ComputeEigenstrainBeamFromVariableTempl<true>;