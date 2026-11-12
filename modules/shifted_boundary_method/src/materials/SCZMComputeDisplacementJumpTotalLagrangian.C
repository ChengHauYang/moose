//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SCZMComputeDisplacementJumpTotalLagrangian.h"
#include "CohesiveZoneModelTools.h"
#include "FactorizedRankTwoTensor.h"

registerMooseObject("ShiftedBoundaryMethodApp", SCZMComputeDisplacementJumpTotalLagrangian);
registerMooseObject("ShiftedBoundaryMethodApp", ADSCZMComputeDisplacementJumpTotalLagrangian);

template <bool is_ad>
InputParameters
SCZMComputeDisplacementJumpTotalLagrangianTempl<is_ad>::validParams()
{
  InputParameters params = SCZMComputeDisplacementJumpBase<is_ad>::validParams();
  params.addClassDescription(
      "Computes the shifted displacement jump in local coordinates for total-Lagrangian "
      "kinematics.");
  return params;
}

template <bool is_ad>
SCZMComputeDisplacementJumpTotalLagrangianTempl<
    is_ad>::SCZMComputeDisplacementJumpTotalLagrangianTempl(const InputParameters & parameters)
  : SCZMComputeDisplacementJumpBase<is_ad>(parameters),
    _F(this->template declareGenericPropertyByName<RankTwoTensor, is_ad>(_base_name + "F_czm")),
    _R(this->template declareGenericPropertyByName<RankTwoTensor, is_ad>(_base_name +
                                                                         "czm_rotation")),
    _czm_reference_rotation(this->template declareGenericPropertyByName<RankTwoTensor, is_ad>(
        _base_name + "czm_reference_rotation"))
{
  for (const auto i : make_range(_ndisp))
    if (this->getVar("displacements", i)->feType().order > SECOND)
      mooseDoOnce(mooseWarning(
          "Total-Lagrangian SCZM Taylor-shifts the displacement jump but evaluates the "
          "deformation gradient on the surrogate interface. Displacement basis orders above "
          "SECOND may therefore be limited by this lower-order deformation-gradient "
          "approximation."));
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpTotalLagrangianTempl<is_ad>::initQpStatefulProperties()
{
  SCZMComputeDisplacementJumpBase<is_ad>::initQpStatefulProperties();
  _displacement_jump_global[_qp] = 0;
  _F[_qp].setToIdentity();
  _R[_qp].setToIdentity();
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpTotalLagrangianTempl<is_ad>::computeLocalDisplacementJump()
{
  _interface_displacement_jump[_qp] =
      _czm_total_rotation[_qp].transpose() * _displacement_jump_global[_qp];
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpTotalLagrangianTempl<is_ad>::computeRotationMatrices()
{
  const auto true_normal =
      this->_shifted ? RealVectorValue(this->_sbm_distance_uo->trueNormal(
                           std::make_pair(this->_current_elem->id(), this->_current_side), _qp))
                     : RealVectorValue(this->_normals[_qp]);
  _czm_reference_rotation[_qp] =
      CohesiveZoneModelTools::computeReferenceRotation<is_ad>(true_normal, this->_mesh.dimension());
  computeFandR();
  _czm_total_rotation[_qp] = _R[_qp] * _czm_reference_rotation[_qp];
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpTotalLagrangianTempl<is_ad>::computeFandR()
{
  // TODO: Taylor-shift F to the true interface using displacement Hessians.
  const auto F = GenericRankTwoTensor<is_ad>::Identity() +
                 GenericRankTwoTensor<is_ad>::initializeFromRows((*this->_grad_disp[0])[_qp],
                                                                 (*this->_grad_disp[1])[_qp],
                                                                 (*this->_grad_disp[2])[_qp]);
  const auto F_neighbor =
      GenericRankTwoTensor<is_ad>::Identity() +
      GenericRankTwoTensor<is_ad>::initializeFromRows((*this->_grad_disp_neighbor[0])[_qp],
                                                      (*this->_grad_disp_neighbor[1])[_qp],
                                                      (*this->_grad_disp_neighbor[2])[_qp]);
  _F[_qp] = 0.5 * (F + F_neighbor);

  for (const auto i : make_range(3))
    for (const auto j : make_range(3))
      if (!std::isfinite(MetaPhysicL::raw_value(_F[_qp](i, j))))
        throw MooseException("SCZM deformation gradient is not finite, reducing time step");

  using FR2T = std::conditional_t<is_ad, ADFactorizedRankTwoTensor, FactorizedRankTwoTensor>;
  const FR2T C = _F[_qp].transpose() * _F[_qp];
  const auto Uinv = MathUtils::sqrt(C).inverse().get();
  _R[_qp] = _F[_qp] * Uinv;
}

template class SCZMComputeDisplacementJumpTotalLagrangianTempl<false>;
template class SCZMComputeDisplacementJumpTotalLagrangianTempl<true>;
