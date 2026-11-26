//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "CZMComputeDisplacementJumpBase.h"
#include "BoundaryShortestDistanceToSurface.h"
#include "SCZMGeneralizedRobinSolver.h"
#include "SCZMWeightedTractionStabilization.h"

#define usingSCZMComputeDisplacementJumpBaseMembers                                                \
  usingInterfaceMaterialMembers;                                                                   \
  using SCZMComputeDisplacementJumpBase<is_ad>::_base_name;                                        \
  using SCZMComputeDisplacementJumpBase<is_ad>::_ndisp;                                            \
  using SCZMComputeDisplacementJumpBase<is_ad>::_disp;                                             \
  using SCZMComputeDisplacementJumpBase<is_ad>::_disp_neighbor;                                    \
  using SCZMComputeDisplacementJumpBase<is_ad>::_displacement_jump_global;                         \
  using SCZMComputeDisplacementJumpBase<is_ad>::_interface_displacement_jump;                      \
  using SCZMComputeDisplacementJumpBase<is_ad>::_czm_total_rotation

/**
 * This interface material class computes the displacement jump in the interface natural coordinate
 * system. The transformation between local and global coordinates shall be defined in
 * computeLocalDisplacementJump.
 */
template <bool is_ad>
class SCZMComputeDisplacementJumpBase : public CZMComputeDisplacementJumpBase<is_ad>
{
public:
  static InputParameters validParams();
  SCZMComputeDisplacementJumpBase(const InputParameters & parameters);

protected:
  using CZMComputeDisplacementJumpBase<is_ad>::_base_name;
  using CZMComputeDisplacementJumpBase<is_ad>::_czm_total_rotation;
  using CZMComputeDisplacementJumpBase<is_ad>::_displacement_jump_global;
  using CZMComputeDisplacementJumpBase<is_ad>::_mesh;
  using CZMComputeDisplacementJumpBase<is_ad>::_ndisp;
  using CZMComputeDisplacementJumpBase<is_ad>::_qp;

  virtual void initialSetup() override final;
  void initQpStatefulProperties() override;
  void computeQpProperties() override;

  /// Computes the jump at the true interface using displacement gradients.
  void computeGlobalDisplacementJump() override;

  /// Computes the rotation using the true-interface normal.
  void computeRotationMatrices() override;

  /**
   * Replaces the shifted jump J_d with the auxiliary jump j of the generalized Robin equation
   * j + f(j) / beta = J_d + T_h / beta, where f is the global cohesive traction and T_h the average
   * bulk traction, and stores the inverse Robin tangent S = (I + df/dj / beta)^{-1}.
   */
  void computeWeightedTractionJump();

  /// Global cohesive traction and its jump derivative at the trial global jump
  Moose::Kokkos::SCZM::CohesiveEvaluation evaluateTrialTraction(const Moose::Kokkos::Real3 & jump);

  /// The Robin parameter, computed from the bulk tangents on first use and then frozen
  Real stabilizationBeta(const RealVectorValue & normal);

  /**
   * Finds the traction-separation and global traction materials that evaluateTrialTraction()
   * re-evaluates, and checks that every property they read is current when they are.
   */
  void findTrialTractionMaterials();

  /// the coupled displacement gradients
  ///@{
  std::vector<const GenericVariableGradient<is_ad> *> _grad_disp;
  std::vector<const GenericVariableGradient<is_ad> *> _grad_disp_neighbor;
  ///@}

  /// @brief  Flag indicating whether shifted terms are included
  bool _shifted;

  /// Distance/normal provider user object
  const BoundaryShortestDistanceToSurface * _sbm_distance_uo = nullptr;

  /// Weighted traction stabilization settings; disabled by default
  const SCZMWeightedTractionStabilization::Settings _stabilization;

  /// Bulk stress on the element and neighbor sides, with stabilization
  ///@{
  const MaterialProperty<RankTwoTensor> * _stress = nullptr;
  const MaterialProperty<RankTwoTensor> * _stress_neighbor = nullptr;
  ///@}

  /// Bulk tangent on the element and neighbor sides, with automatic beta
  ///@{
  const MaterialProperty<RankFourTensor> * _tangent = nullptr;
  const MaterialProperty<RankFourTensor> * _tangent_neighbor = nullptr;
  ///@}

  /// Current and old frozen Robin parameter, with automatic beta; zero until first computed
  ///@{
  MaterialProperty<Real> * _beta = nullptr;
  const MaterialProperty<Real> * _beta_old = nullptr;
  ///@}

  /// Inverse Robin tangent S, with which the kernels weight the cohesive traction derivatives
  MaterialProperty<RankTwoTensor> * _inverse_robin_tangent = nullptr;

  /// Global cohesive traction assembled by the kernels and its global jump derivative
  ///@{
  const MaterialProperty<RealVectorValue> * _trial_traction = nullptr;
  const MaterialProperty<RankTwoTensor> * _trial_dtraction_djump = nullptr;
  ///@}

  /// Materials computing the local and the global cohesive traction from the jump
  ///@{
  MaterialBase * _traction_separation_material = nullptr;
  MaterialBase * _global_traction_material = nullptr;
  ///@}
};
