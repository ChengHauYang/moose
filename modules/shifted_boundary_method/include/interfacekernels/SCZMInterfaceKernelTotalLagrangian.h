//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "SCZMInterfaceKernelBase.h"

#include "libmesh/fe_base.h"
#include "libmesh/quadrature.h"

/** Shifted cohesive interface kernel for the total-Lagrangian formulation. */
template <bool is_ad>
class SCZMInterfaceKernelTotalLagrangianTempl : public SCZMInterfaceKernelBaseTempl<is_ad>
{
public:
  static InputParameters validParams();
  SCZMInterfaceKernelTotalLagrangianTempl(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void jacobianSetup() override;

protected:
  GenericReal<is_ad> computeQpResidual(Moose::DGResidualType type) override;
  Real computeQpJacobian(Moose::DGJacobianType type) override;
  Real computeQpOffDiagJacobian(Moose::DGJacobianType type, unsigned int jvar) override;

  Real computeDResidualDDisplacement(unsigned int component_j, Moose::DGJacobianType type) const;
  Real computeDResidualDDeformationGradient(unsigned int component_j,
                                            Moose::DGJacobianType type) const;
  Real calculateDirectionalCorrectionJacobian(unsigned int ivar,
                                              unsigned int jvar,
                                              Moose::DGJacobianType type);

  /**
   * Contribution of the F-bar deformation gradient B = (det(M) / det(U))^(1/3) U to
   * n_t . dP / du_j for the row-side stress P = P(U, B), given the stabilized tangent C_B = dP/dB,
   * U, M, and the local and element-volume averaged trial gradients.
   */
  Real fbarDirectionalCorrection(unsigned int ivar,
                                 unsigned int jvar,
                                 const RankFourTensor & stabilized_tangent,
                                 const RankTwoTensor & U,
                                 const RankTwoTensor & M,
                                 const RealVectorValue & normal_tangent,
                                 const RealVectorValue & grad_phi,
                                 const RealVectorValue & avg_grad_phi) const;

  /**
   * Average of the trial-function gradients over the true reference volume of elem, cached until
   * elem changes. The volume quadrature matches the one used for the average deformation gradient.
   */
  const std::vector<RealVectorValue> & volumeAverageGradPhi(const Elem * elem,
                                                            const Elem *& cached_elem,
                                                            std::vector<RealVectorValue> & cache);

  /// The PK1 stress tensor on the element and neighbor sides, when directional correction is used
  ///@{
  const GenericMaterialProperty<RankTwoTensor, is_ad> * _stress;
  const GenericMaterialProperty<RankTwoTensor, is_ad> * _stress_neighbor;
  ///@}

  /// Non-AD data used by the hand-coded Jacobian
  ///@{
  std::vector<unsigned int> _disp_var;
  std::vector<MooseVariable *> _vars;
  const MaterialProperty<RankTwoTensor> * _dtraction_djump_global;
  const MaterialProperty<RankThreeTensor> * _dPK1traction_dF;
  const MaterialProperty<RankFourTensor> * _Jacobian_mult;
  const MaterialProperty<RankFourTensor> * _Jacobian_mult_neighbor;
  ///@}

  /// Total-mode F-bar data on the element and neighbor sides, used with
  /// volumetric_locking_correction: the stabilized tangent dP/dB at fixed U, the physical
  /// deformation gradient U, and its owning-element volume average M
  ///@{
  const MaterialProperty<RankFourTensor> * _stabilized_tangent;
  const MaterialProperty<RankFourTensor> * _stabilized_tangent_neighbor;
  const MaterialProperty<RankTwoTensor> * _physical_deformation_gradient;
  const MaterialProperty<RankTwoTensor> * _physical_deformation_gradient_neighbor;
  const MaterialProperty<RankTwoTensor> * _average_deformation_gradient;
  const MaterialProperty<RankTwoTensor> * _average_deformation_gradient_neighbor;
  ///@}

  /// Volume quadrature and finite element used to average trial gradients over an element
  ///@{
  std::unique_ptr<libMesh::QBase> _volume_qrule;
  std::unique_ptr<libMesh::FEBase> _volume_fe;
  ///@}

  /// Cached element-volume averages of the trial gradients on the element and neighbor sides
  ///@{
  const Elem * _avg_grad_phi_elem;
  const Elem * _avg_grad_phi_neighbor_elem;
  std::vector<RealVectorValue> _avg_grad_phi;
  std::vector<RealVectorValue> _avg_grad_phi_neighbor;
  ///@}

  /// Whether to add the directional correction term
  const bool _directional_correction;

  /// Whether the directional correction uses the total-mode F-bar PK1 stress P(U, B)
  bool _volumetric_locking_correction;

  using SCZMInterfaceKernelBaseTempl<is_ad>::_base_name;
  using SCZMInterfaceKernelBaseTempl<is_ad>::_component;
  using SCZMInterfaceKernelBaseTempl<is_ad>::_ndisp;
  using SCZMInterfaceKernelBaseTempl<is_ad>::_shifted;
  using SCZMInterfaceKernelBaseTempl<is_ad>::_normals;
  using SCZMInterfaceKernelBaseTempl<is_ad>::surrogateDistance;
  using SCZMInterfaceKernelBaseTempl<is_ad>::trueNormal;
  usingGenericInterfaceKernelMembers;
};

using SCZMInterfaceKernelTotalLagrangian = SCZMInterfaceKernelTotalLagrangianTempl<false>;
using ADSCZMInterfaceKernelTotalLagrangian = SCZMInterfaceKernelTotalLagrangianTempl<true>;
