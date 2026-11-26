//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SCZMComputeDisplacementJumpBase.h"
#include "CohesiveZoneModelTools.h"
#include "BoundaryShortestDistanceToSurface.h"

template <bool is_ad>
InputParameters
SCZMComputeDisplacementJumpBase<is_ad>::validParams()
{
  InputParameters params = CZMComputeDisplacementJumpBase<is_ad>::validParams();
  params.addClassDescription(
      "Base class used to compute the shifted displacement jump across a czm "
      "interface in local coordinates");

  // no shifted testing
  params.addParam<bool>(
      "no_shifted",
      false,
      "Disable shifted integration corrections to provide a non-shifted baseline for verification "
      "and comparison.");

  params.addParam<UserObjectName>(
      "sbm_distance_uo",
      "UserObject that provides signed distance and normal vector calculations.");

  SCZMWeightedTractionStabilization::addParams(params);
  params.addParam<MaterialPropertyName>(
      "stress", "stress", "Name of the bulk stress averaged by weighted traction stabilization.");
  params.addParam<MaterialPropertyName>(
      "tangent",
      "Jacobian_mult",
      "Name of the bulk tangent whose normal stiffness sets the automatic stabilization beta.");
  // Must match the traction assembled by the interface kernel
  params.addPrivateParam<std::string>("traction_global_name", "traction_global");

  return params;
}

template <bool is_ad>
SCZMComputeDisplacementJumpBase<is_ad>::SCZMComputeDisplacementJumpBase(
    const InputParameters & parameters)
  : CZMComputeDisplacementJumpBase<is_ad>(parameters),
    _grad_disp(3),
    _grad_disp_neighbor(3),
    _shifted(!this->template getParam<bool>("no_shifted")),
    _sbm_distance_uo(nullptr),
    _stabilization(SCZMWeightedTractionStabilization::settings(*this))
{
  if (_stabilization.enabled)
  {
    if constexpr (is_ad)
      this->paramError("stabilization",
                       "Weighted traction stabilization requires the non-AD shifted cohesive zone "
                       "objects.");
    else
    {
      const auto stress = _base_name + this->template getParam<MaterialPropertyName>("stress");
      _stress = &this->template getMaterialPropertyByName<RankTwoTensor>(stress);
      _stress_neighbor = &this->template getNeighborMaterialPropertyByName<RankTwoTensor>(stress);
      const auto tangent = _base_name + this->template getParam<MaterialPropertyName>("tangent");
      _tangent = &this->template getMaterialPropertyByName<RankFourTensor>(tangent);
      _tangent_neighbor =
          &this->template getNeighborMaterialPropertyByName<RankFourTensor>(tangent);
      _beta = &this->template declarePropertyByName<Real>(_base_name + "stabilization_beta");
      _beta_old =
          &this->template getMaterialPropertyOldByName<Real>(_base_name + "stabilization_beta");
      _inverse_robin_tangent = &this->template declarePropertyByName<RankTwoTensor>(
          _base_name + "inverse_robin_tangent");

      // The traction materials consume this material's jump, so requesting their properties
      // through MaterialPropertyInterface would make the dependency cyclic
      auto & material_data = this->materialData();
      _trial_traction = &material_data.template getProperty<RealVectorValue>(
          _base_name + this->template getParam<std::string>("traction_global_name"), 0, *this);
      _trial_dtraction_djump = &material_data.template getProperty<RankTwoTensor>(
          _base_name + "dtraction_djump_global", 0, *this);
    }
  }

  for (const auto i : make_range(_ndisp))
  {
    _grad_disp[i] = &this->template coupledGenericGradient<is_ad>("displacements", i);
    _grad_disp_neighbor[i] =
        &this->template coupledGenericNeighborGradient<is_ad>("displacements", i);
  }

  for (const auto i : make_range(_ndisp, 3u))
    if constexpr (is_ad)
    {
      _grad_disp[i] = &this->_ad_grad_zero;
      _grad_disp_neighbor[i] = &this->_ad_grad_zero;
    }
    else
    {
      _grad_disp[i] = &this->_grad_zero;
      _grad_disp_neighbor[i] = &this->_grad_zero;
    }
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpBase<is_ad>::initialSetup()
{
  if (_shifted)
  {
    if (!this->isParamSetByUser("sbm_distance_uo"))
      mooseError("Please provide 'sbm_distance_uo' when using shifted terms.");
    _sbm_distance_uo =
        &this->template getUserObject<BoundaryShortestDistanceToSurface>("sbm_distance_uo");
  }

  if (_stabilization.enabled)
    findTrialTractionMaterials();
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpBase<is_ad>::findTrialTractionMaterials()
{
  const auto & warehouse = this->_fe_problem.getInterfaceMaterialsWarehouse();
  const auto supplier = [&](const std::string & value, const std::string & derivative)
  {
    MaterialBase * found = nullptr;
    for (const auto bid : this->boundaryIDs())
      for (const auto & material : warehouse.getActiveBoundaryObjects(bid, this->_tid))
        if (material->getSuppliedItems().count(value))
        {
          if (found && found != material.get())
            mooseError("Weighted traction stabilization on '",
                       this->name(),
                       "' found two materials supplying '",
                       value,
                       "' on its interface: '",
                       found->name(),
                       "' and '",
                       material->name(),
                       "'.");
          found = material.get();
        }
    if (!found)
      mooseError("Weighted traction stabilization on '",
                 this->name(),
                 "' found no interface material supplying '",
                 value,
                 "'.");
    // The trial evaluation recomputes both from the jump within one material
    if (!found->getSuppliedItems().count(derivative))
      mooseError("Weighted traction stabilization on '",
                 this->name(),
                 "' requires the material '",
                 found->name(),
                 "' supplying '",
                 value,
                 "' to also supply '",
                 derivative,
                 "'. A traction-separation law evaluated by an interface [NEML2] block requires "
                 "the Kokkos objects ('use_kokkos = true').");
    return found;
  };
  _traction_separation_material =
      supplier(_base_name + "interface_traction", _base_name + "dinterface_traction_djump");
  _global_traction_material =
      supplier(_base_name + this->template getParam<std::string>("traction_global_name"),
               _base_name + "dtraction_djump_global");

  // The trial materials are recomputed before the materials sorted after this one, which must
  // therefore supply none of their inputs
  for (const auto bid : this->boundaryIDs())
  {
    const auto & materials = warehouse.getActiveBoundaryObjects(bid, this->_tid);
    bool after_this = false;
    for (const auto & material : materials)
    {
      if (material.get() == this)
      {
        after_this = true;
        continue;
      }
      if (!after_this || material.get() == _traction_separation_material ||
          material.get() == _global_traction_material)
        continue;
      for (auto * const trial : {_traction_separation_material, _global_traction_material})
        for (const auto & property : trial->getRequestedItems())
          if (material->getSuppliedItems().count(property))
            mooseError("Weighted traction stabilization on '",
                       this->name(),
                       "' re-evaluates '",
                       trial->name(),
                       "' at trial jumps, but its input '",
                       property,
                       "' is supplied by '",
                       material->name(),
                       "', which is computed after '",
                       this->name(),
                       "'.");
    }
  }
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpBase<is_ad>::initQpStatefulProperties()
{
  CZMComputeDisplacementJumpBase<is_ad>::initQpStatefulProperties();
  if (_beta)
    (*_beta)[_qp] = 0.0;
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpBase<is_ad>::computeQpProperties()
{
  CZMComputeDisplacementJumpBase<is_ad>::computeQpProperties();
  if (_stabilization.enabled)
    computeWeightedTractionJump();
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpBase<is_ad>::computeWeightedTractionJump()
{
  if constexpr (!is_ad)
  {
    using namespace Moose::Kokkos::SCZM;
    const auto normal =
        _shifted ? RealVectorValue(_sbm_distance_uo->trueNormal(
                       std::make_pair(this->_current_elem->id(), this->_current_side), _qp))
                 : RealVectorValue(this->_normals[_qp]);

    Moose::Kokkos::Real3 shifted_jump{};
    Moose::Kokkos::Real3 bulk_traction{};
    for (const auto i : make_range(_ndisp))
    {
      shifted_jump.v[i] = _displacement_jump_global[_qp](i);
      for (const auto J : make_range(_ndisp))
        bulk_traction.v[i] +=
            0.5 * ((*_stress)[_qp](i, J) + (*_stress_neighbor)[_qp](i, J)) * normal(J);
    }

    struct Evaluator
    {
      CohesiveEvaluation evaluate(const Moose::Kokkos::Real3 & jump, unsigned int) const
      {
        return material.evaluateTrialTraction(jump);
      }
      SCZMComputeDisplacementJumpBase<is_ad> & material;
    };
    auto & S = (*_inverse_robin_tangent)[_qp];
    const Real beta = stabilizationBeta(normal);
    if (beta == 0.0)
    {
      // No beta yet on an initial evaluation: keep the shifted jump
      S = RankTwoTensor::Identity();
      return;
    }

    const auto result = GeneralizedRobinSolver::solve(
        Evaluator{*this}, shifted_jump, bulk_traction, beta, _ndisp, false);
    if (!result.converged)
      throw MooseException("SCZM weighted traction stabilization did not converge, reducing time "
                           "step");

    // The traction materials sorted after this one evaluate the cohesive law at j
    S.zero();
    for (const auto i : make_range(_ndisp))
    {
      _displacement_jump_global[_qp](i) = result.auxiliary_jump.v[i];
      for (const auto j : make_range(_ndisp))
        S(i, j) = result.inverse_robin_tangent.a[i][j];
    }
    this->computeLocalDisplacementJump();
  }
}

template <bool is_ad>
Moose::Kokkos::SCZM::CohesiveEvaluation
SCZMComputeDisplacementJumpBase<is_ad>::evaluateTrialTraction(const Moose::Kokkos::Real3 & jump)
{
  for (const auto i : make_range(_ndisp))
    _displacement_jump_global[_qp](i) = jump.v[i];
  this->computeLocalDisplacementJump();
  _traction_separation_material->computePropertiesAtQp(_qp);
  _global_traction_material->computePropertiesAtQp(_qp);

  Moose::Kokkos::SCZM::CohesiveEvaluation evaluation;
  for (const auto i : make_range(_ndisp))
  {
    evaluation.traction.v[i] = (*_trial_traction)[_qp](i);
    for (const auto j : make_range(_ndisp))
      evaluation.jump_tangent.a[i][j] = (*_trial_dtraction_djump)[_qp](i, j);
  }
  return evaluation;
}

template <bool is_ad>
Real
SCZMComputeDisplacementJumpBase<is_ad>::stabilizationBeta(const RealVectorValue & normal)
{
  // Stored every evaluation so that the frozen value is carried to the next time step
  auto & beta = (*_beta)[_qp];
  if ((*_beta_old)[_qp] > 0.0)
    beta = (*_beta_old)[_qp];
  else if (!(beta > 0.0))
  {
    Real face_area = 0.0;
    for (const auto qp : make_range(this->_qrule->n_points()))
      face_area += this->_JxW[qp];
    beta = Moose::Kokkos::SCZM::automaticBeta(
        _stabilization.gamma,
        face_area,
        Moose::Kokkos::SCZM::normalStiffness((*_tangent)[_qp], normal, _ndisp),
        this->_current_elem->volume(),
        Moose::Kokkos::SCZM::normalStiffness((*_tangent_neighbor)[_qp], normal, _ndisp),
        this->_neighbor_elem->volume());
    if (!(beta > 0.0) || !std::isfinite(beta))
    {
      // A user object producing the bulk tangent, such as a NEML2 model executor whose input
      // gathering evaluates these interface materials, may not have run yet on EXEC_INITIAL. Leave
      // beta unset so that the first evaluation after it runs derives and freezes beta.
      if (this->_fe_problem.getCurrentExecuteOnFlag() == EXEC_INITIAL)
        beta = 0.0;
      else
        mooseError("SCZM stabilization beta must be finite and positive; check the normal "
                   "directional bulk tangents and adjacent element volumes.");
    }
  }
  return beta;
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpBase<is_ad>::computeGlobalDisplacementJump()
{
  CZMComputeDisplacementJumpBase<is_ad>::computeGlobalDisplacementJump();

  if (_shifted)
  {
    const auto d = _sbm_distance_uo->surrogateDistance(
        std::make_pair(this->_current_elem->id(), this->_current_side), _qp);
    for (const auto i : make_range(_ndisp))
      _displacement_jump_global[_qp](i) +=
          ((*_grad_disp_neighbor[i])[_qp] - (*_grad_disp[i])[_qp]) * d;
  }
}

template <bool is_ad>
void
SCZMComputeDisplacementJumpBase<is_ad>::computeRotationMatrices()
{
  const auto true_normal =
      _shifted ? RealVectorValue(_sbm_distance_uo->trueNormal(
                     std::make_pair(this->_current_elem->id(), this->_current_side), _qp))
               : RealVectorValue(this->_normals[_qp]);

  _czm_total_rotation[_qp] =
      CohesiveZoneModelTools::computeReferenceRotation<is_ad>(true_normal, _mesh.dimension());
}

template class SCZMComputeDisplacementJumpBase<false>;
template class SCZMComputeDisplacementJumpBase<true>;
