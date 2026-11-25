//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosSCZMInterfaceKernelTotalLagrangian.h"

#include "KokkosInterfaceDatum.h"
#include "FEProblemBase.h"

#include "libmesh/fe_map.h"

#include <cmath>
#include <vector>

registerKokkosInterfaceKernel("ShiftedBoundaryMethodApp", KokkosSCZMInterfaceKernelTotalLagrangian);

InputParameters
KokkosSCZMInterfaceKernelTotalLagrangian::validParams()
{
  auto params = ShiftedInterfaceKernel::validParams();
  params.addRequiredParam<unsigned int>("component",
                                        "The displacement component assembled by this kernel.");
  params.addRequiredCoupledVar("displacements", "The displacement variables.");
  params.addParam<std::string>("base_name", "Material property base name.");
  params.addParam<MaterialPropertyName>(
      "traction_global", "PK1traction", "Global PK1 traction material property.");
  params.addParam<MaterialPropertyName>(
      "dtraction_djump_global",
      "dtraction_djump_global",
      "Derivative of global PK1 traction with respect to global jump.");
  params.addParam<MaterialPropertyName>(
      "dPK1traction_dF",
      "dPK1traction_dF",
      "Derivative of global PK1 traction with respect to deformation gradient.");
  params.addParam<bool>(
      "no_shifted",
      false,
      "Disable shifted terms. When enabled the base traction omits the area correction and the "
      "directional correction, and the traction-jump Jacobian uses the natural test functions.");
  params.addParam<bool>("directional_correction", true, "Add the directional correction terms.");
  params.addParam<MooseEnum>(
      "stress_recovery",
      MooseEnum("none element_linear", "none"),
      "Optional element-local affine recovery of each side's PK1 stress from its volume quadrature "
      "points to the true-interface quadrature point.");
  params.addParam<bool>(
      "volumetric_locking_correction",
      false,
      "Use the total-mode F-bar PK1 stress P(U, B) in the directional correction. The 'tangent' "
      "property is then d(P)/d(U) at fixed B, and the Jacobian adds the local and element-volume "
      "F-bar terms through B = (det(M) / det(U))^(1/3) U.");
  params.addParam<MaterialPropertyName>(
      "stress", "pk1_stress", "Interface PK1 stress material property.");
  params.addParam<MaterialPropertyName>(
      "tangent", "pk1_jacobian", "Interface constitutive tangent material property (d(PK1)/d(F)).");
  params.addParam<MaterialPropertyName>(
      "volume_stress", "pk1_stress", "Volume PK1 stress used by element-linear recovery.");
  params.addParam<MaterialPropertyName>(
      "volume_tangent",
      "dpk1_d_grad_u",
      "Complete local volume derivative d(PK1)/d(grad(u)) used by element-linear recovery.");
  params.addParam<MaterialPropertyName>("volume_F_avg_invT",
                                        "F_avg_invT",
                                        "Inverse transpose of the element-average deformation "
                                        "gradient used by recovered F-bar stress.");
  params.addParam<MaterialPropertyName>(
      "volume_fbar_A", "fbar_A", "Volume F-bar A(qp) tensor used by recovered F-bar stress.");
  params.addParam<MaterialPropertyName>(
      "volume_fbar_gamma_over_3",
      "fbar_gamma_over_3",
      "Volume F-bar gamma/3 scalar used by recovered F-bar stress.");
  params.addParam<MaterialPropertyName>(
      "stabilized_tangent",
      "stabilized_pk1_jacobian",
      "Derivative of the PK1 stress with respect to the F-bar deformation gradient B at fixed "
      "physical deformation gradient U, used with volumetric_locking_correction.");
  params.addParam<MaterialPropertyName>(
      "physical_deformation_gradient",
      "physical_deformation_gradient",
      "Physical deformation gradient U, used with volumetric_locking_correction.");
  params.addParam<MaterialPropertyName>(
      "average_deformation_gradient",
      "average_deformation_gradient",
      "Average M of the physical deformation gradient over the owning element volume, used with "
      "volumetric_locking_correction.");
  params.addParam<MooseEnum>("tangent_definition",
                             MooseEnum("auto stress_wrt_strain pk1_wrt_deformation_gradient",
                                       "pk1_wrt_deformation_gradient"),
                             "Mathematical definition of the tangent material property.");
  SCZMWeightedTractionStabilization::addParams(params);
  params.addClassDescription("Kokkos shifted cohesive-zone interface kernel for total-Lagrangian "
                             "finite-strain displacements.");
  return params;
}

KokkosSCZMInterfaceKernelTotalLagrangian::KokkosSCZMInterfaceKernelTotalLagrangian(
    const InputParameters & parameters)
  : ShiftedInterfaceKernel(parameters),
    _component(getParam<unsigned int>("component")),
    _ndisp(coupledComponents("displacements")),
    _shifted(!getParam<bool>("no_shifted")),
    _directional_correction(getParam<bool>("directional_correction")),
    _volumetric_locking_correction(getParam<bool>("volumetric_locking_correction")),
    _element_linear_recovery(getParam<MooseEnum>("stress_recovery") == "element_linear"),
    _stabilization_weights(SCZMWeightedTractionStabilization::weights(*this)),
    _traction_global(getKokkosMaterialPropertyByName<Real, 1>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("traction_global"))),
    _dtraction_djump_global(getKokkosMaterialPropertyByName<Real, 2>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("dtraction_djump_global"))),
    _dPK1traction_dF(getKokkosMaterialPropertyByName<Real, 3>(
        (isParamValid("base_name") && !getParam<std::string>("base_name").empty()
             ? getParam<std::string>("base_name") + "_"
             : "") +
        getParam<MaterialPropertyName>("dPK1traction_dF")))
{
  if (_ndisp != _mesh.dimension())
    paramError("displacements", "Number of displacements must match problem dimension.");
  if (_ndisp < 1 || _ndisp > 3)
    paramError("displacements", "One, two, or three displacement variables are required.");
  if (_component >= _ndisp)
    paramError("component", "Component must be smaller than the number of displacements.");

  _disp_vars.create(_ndisp);
  for (unsigned int i = 0; i < _ndisp; ++i)
    _disp_vars[i] = coupled("displacements", i);
  _disp_vars.copyToDevice();

  if (_shifted && !isParamValid("sbm_distance_uo"))
    paramError("sbm_distance_uo",
               "The 'sbm_distance_uo' parameter is required unless 'no_shifted = true'.");
  if (_element_linear_recovery && !_stabilization_weights.enabled)
    paramError("stress_recovery",
               "'element_linear' requires weighted traction stabilization because recovery applies "
               "to its bulk PK1 stress contribution.");
  if (_element_linear_recovery && !_shifted)
    paramError("stress_recovery", "'element_linear' requires the true-interface mapping.");

  if (_element_linear_recovery)
  {
    auto & volume_material_data =
        _fe_problem.getKokkosMaterialData(Moose::BLOCK_MATERIAL_DATA, this);
    const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                            ? getParam<std::string>("base_name") + "_"
                            : "";
    const auto volume_stress = prefix + getParam<MaterialPropertyName>("volume_stress");
    const auto volume_tangent = prefix + getParam<MaterialPropertyName>("volume_tangent");
    _volume_stress = getKokkosMaterialPropertyByName<Real, 2>(volume_stress, volume_material_data);
    _volume_tangent =
        getKokkosMaterialPropertyByName<Real, 4>(volume_tangent, volume_material_data);
    auto & volume_storage = _fe_problem.getKokkosMaterialPropertyStorage();
    volume_storage.requestGhostedProperty(volume_stress);
    volume_storage.requestGhostedProperty(volume_tangent);
    if (_volumetric_locking_correction)
    {
      const auto F_avg_invT = prefix + getParam<MaterialPropertyName>("volume_F_avg_invT");
      const auto fbar_A = prefix + getParam<MaterialPropertyName>("volume_fbar_A");
      const auto fbar_gamma_over_3 =
          prefix + getParam<MaterialPropertyName>("volume_fbar_gamma_over_3");
      _volume_F_avg_invT =
          getKokkosMaterialPropertyByName<Real, 2>(F_avg_invT, volume_material_data);
      _volume_fbar_A = getKokkosMaterialPropertyByName<Real, 2>(fbar_A, volume_material_data);
      _volume_fbar_gamma_over_3 =
          getKokkosMaterialPropertyByName<Real, 0>(fbar_gamma_over_3, volume_material_data);
      volume_storage.requestGhostedProperty(F_avg_invT);
      volume_storage.requestGhostedProperty(fbar_A);
      volume_storage.requestGhostedProperty(fbar_gamma_over_3);
    }
  }

  // Recovery replaces both true-interface stabilization stress and surrogate-face directional
  // stress
  if (!_element_linear_recovery &&
      ((_shifted && _directional_correction) || _stabilization_weights.enabled))
  {
    const auto prefix = isParamValid("base_name") && !getParam<std::string>("base_name").empty()
                            ? getParam<std::string>("base_name") + "_"
                            : "";
    _stress =
        getKokkosMaterialPropertyByName<Real, 2>(prefix + getParam<MaterialPropertyName>("stress"));
    _stress_neighbor = getKokkosNeighborMaterialPropertyByName<Real, 2>(
        prefix + getParam<MaterialPropertyName>("stress"));
    _tangent = getKokkosMaterialPropertyByName<Real, 4>(prefix +
                                                        getParam<MaterialPropertyName>("tangent"));
    _tangent_neighbor = getKokkosNeighborMaterialPropertyByName<Real, 4>(
        prefix + getParam<MaterialPropertyName>("tangent"));

    if (_volumetric_locking_correction)
    {
      if (getParam<MooseEnum>("tangent_definition") != "pk1_wrt_deformation_gradient")
        paramError("tangent_definition",
                   "The total-Lagrangian F-bar correction requires "
                   "'pk1_wrt_deformation_gradient'.");

      const auto stabilized_tangent = prefix + getParam<MaterialPropertyName>("stabilized_tangent");
      const auto physical =
          prefix + getParam<MaterialPropertyName>("physical_deformation_gradient");
      const auto average = prefix + getParam<MaterialPropertyName>("average_deformation_gradient");
      _stabilized_tangent = getKokkosMaterialPropertyByName<Real, 4>(stabilized_tangent);
      _stabilized_tangent_neighbor =
          getKokkosNeighborMaterialPropertyByName<Real, 4>(stabilized_tangent);
      _physical_deformation_gradient = getKokkosMaterialPropertyByName<Real, 2>(physical);
      _physical_deformation_gradient_neighbor =
          getKokkosNeighborMaterialPropertyByName<Real, 2>(physical);
      _average_deformation_gradient = getKokkosMaterialPropertyByName<Real, 2>(average);
      _average_deformation_gradient_neighbor =
          getKokkosNeighborMaterialPropertyByName<Real, 2>(average);
    }
  }
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::computeResidual()
{
  ensureSurrogateInterfaceMappingReady();
  initializeRecovery();
  InterfaceKernel::computeResidual();
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::computeJacobian()
{
  ensureSurrogateInterfaceMappingReady();
  initializeRecovery();
  InterfaceKernel::computeJacobian();
}

void
KokkosSCZMInterfaceKernelTotalLagrangian::initializeRecovery()
{
  if (!_element_linear_recovery || _recovery_initialized)
    return;

  dof_id_type n_interface_qps = 0;
  dof_id_type n_weights = 0;
  Real maximum_amplification = 0.0;
  for (const auto & face : kokkosInterfaceFaces())
  {
    n_interface_qps += face.n_qps;
    auto elem_face = Moose::Kokkos::makePrimaryInterfaceDatum<AssemblyDatum>(
        face, kokkosAssembly(), kokkosSystems(), _kokkos_var, _kokkos_var.var());
    auto neighbor_face = Moose::Kokkos::makeNeighborInterfaceDatum<AssemblyDatum>(
        face, kokkosAssembly(), kokkosSystems(), _kokkos_neighbor_var, _kokkos_neighbor_var.var());
    AssemblyDatum elem_volume(elem_face, face.elem, libMesh::invalid_uint, face.neighbor, true);
    AssemblyDatum neighbor_volume(
        neighbor_face, face.neighbor, libMesh::invalid_uint, face.elem, true);
    // Recover independently to both the true-interface and surrogate-face points.
    n_weights += 2 * face.n_qps * (elem_volume.n_qps() + neighbor_volume.n_qps());
  }

  _recovery_offsets.create(4 * n_interface_qps);
  _recovery_weights.create(n_weights);

  dof_id_type next_weight = 0;
  for (const auto & face : kokkosInterfaceFaces())
  {
    auto elem_face = Moose::Kokkos::makePrimaryInterfaceDatum<AssemblyDatum>(
        face, kokkosAssembly(), kokkosSystems(), _kokkos_var, _kokkos_var.var());
    auto neighbor_face = Moose::Kokkos::makeNeighborInterfaceDatum<AssemblyDatum>(
        face, kokkosAssembly(), kokkosSystems(), _kokkos_neighbor_var, _kokkos_neighbor_var.var());
    AssemblyDatum elem_volume(elem_face, face.elem, libMesh::invalid_uint, face.neighbor, true);
    AssemblyDatum neighbor_volume(
        neighbor_face, face.neighbor, libMesh::invalid_uint, face.elem, true);

    for (const auto qp : make_range(face.n_qps))
    {
      const auto map_index = _qp_offsets(face.side, face.elem) + qp;
      const auto surrogate_point = elem_face.q_point(qp);
      const auto distance = _surrogate_distance_vectors[map_index];
      const Point true_point(surrogate_point(0) + distance(0),
                             surrogate_point(1) + distance(1),
                             surrogate_point(2) + distance(2));
      for (const auto component : make_range(_mesh.dimension()))
        if (!std::isfinite(true_point(component)))
          paramError("stress_recovery",
                     "The true-interface point for element ",
                     face.elem_id,
                     " is non-finite.");

      for (const auto target_index : make_range(2))
      {
        const Point target_point =
            target_index == 0 ? true_point
                              : Point(surrogate_point(0), surrogate_point(1), surrogate_point(2));
        for (const auto side : make_range(2))
        {
          auto & volume = side == 0 ? elem_volume : neighbor_volume;
          const auto * const elem = _mesh.elemPtr(side == 0 ? face.elem_id : face.neighbor_id);
          const auto n_qps = volume.n_qps();
          const auto offset_index = 4 * map_index + 2 * target_index + side;
          _recovery_offsets[offset_index] = next_weight;

          RealEigenVector weights(n_qps);
          std::vector<Point> reproduction_points(n_qps);
          const bool analytical_hex8 = elem->type() == HEX8 && n_qps == 8;
          Point reproduction_target = target_point;
          if (analytical_hex8)
          {
            const Point target = libMesh::FEMap::inverse_map(elem->dim(), elem, target_point);
            reproduction_target = target;
            for (const auto component : make_range(elem->dim()))
              if (!std::isfinite(target(component)))
                paramError("stress_recovery",
                           "The inverse-mapped true-interface point for element ",
                           elem->id(),
                           " is non-finite.");
            for (const auto volume_qp : make_range(n_qps))
            {
              const auto x = volume.q_point(volume_qp);
              for (const auto component : make_range(elem->dim()))
                if (!std::isfinite(x(component)))
                  paramError("stress_recovery",
                             "Element ",
                             elem->id(),
                             " has a non-finite volume quadrature point.");
              const Point physical_point(x(0), x(1), x(2));
              const Point sample = libMesh::FEMap::inverse_map(elem->dim(), elem, physical_point);
              reproduction_points[volume_qp] = sample;
              for (const auto component : make_range(elem->dim()))
                if (!std::isfinite(sample(component)))
                  paramError("stress_recovery",
                             "A volume quadrature point inverse map for element ",
                             elem->id(),
                             " is non-finite.");
              weights(volume_qp) = (1.0 + 3.0 * (target(0) * sample(0) + target(1) * sample(1) +
                                                 target(2) * sample(2))) /
                                   8.0;
            }
          }
          else
          {
            const auto basis_size = elem->dim() + 1;
            if (n_qps < basis_size)
              paramError("stress_recovery",
                         "Element ",
                         elem->id(),
                         " has ",
                         n_qps,
                         " volume quadrature points, fewer than the ",
                         basis_size,
                         " samples required for affine stress recovery.");

            RealEigenMatrix design(n_qps, basis_size);
            for (const auto volume_qp : make_range(n_qps))
            {
              const auto point = volume.q_point(volume_qp);
              reproduction_points[volume_qp] = Point(point(0), point(1), point(2));
              design(volume_qp, 0) = 1.0;
              for (const auto component : make_range(elem->dim()))
              {
                if (!std::isfinite(point(component)))
                  paramError("stress_recovery",
                             "Element ",
                             elem->id(),
                             " has a non-finite volume quadrature point.");
                design(volume_qp, component + 1) = point(component) - target_point(component);
              }
            }

            const auto decomposition = design.transpose().completeOrthogonalDecomposition();
            if (decomposition.rank() != basis_size)
              paramError("stress_recovery",
                         "Volume quadrature points on element ",
                         elem->id(),
                         " are rank deficient for affine stress recovery.");
            RealEigenVector target = RealEigenVector::Zero(basis_size);
            target(0) = 1.0;
            weights = decomposition.solve(target);
          }

          Real coordinate_scale = std::max(1.0, reproduction_target.norm());
          for (const auto volume_qp : make_range(n_qps))
            coordinate_scale = std::max(coordinate_scale, reproduction_points[volume_qp].norm());
          // This is the same scale as libMesh's geometric tolerance, enlarged to cover the
          // least-squares solve and inverse maps used to construct the weights.
          const Real constant_tolerance = 100.0 * libMesh::TOLERANCE;
          const Real affine_tolerance = constant_tolerance * coordinate_scale;
          Real constant = 0.0;
          Point affine;
          Real amplification = 0.0;
          for (const auto volume_qp : make_range(n_qps))
          {
            const auto & point = reproduction_points[volume_qp];
            const Real weight = weights(volume_qp);
            if (!std::isfinite(weight))
              paramError("stress_recovery",
                         "Affine recovery produced a non-finite weight on element ",
                         elem->id(),
                         ".");
            constant += weight;
            amplification += std::abs(weight);
            for (const auto component : make_range(elem->dim()))
              affine(component) += weight * point(component);
            _recovery_weights[next_weight++] = weight;
          }

          if (std::abs(constant - 1.0) > constant_tolerance)
            paramError("stress_recovery",
                       "Affine recovery failed constant reproduction on element ",
                       elem->id(),
                       ".");
          for (const auto component : make_range(elem->dim()))
            if (std::abs(affine(component) - reproduction_target(component)) > affine_tolerance)
              paramError("stress_recovery",
                         "Affine recovery failed linear reproduction on element ",
                         elem->id(),
                         ".");
          if (!std::isfinite(amplification))
            paramError("stress_recovery",
                       "Affine recovery has non-finite weight amplification on element ",
                       elem->id(),
                       ".");
          maximum_amplification = std::max(maximum_amplification, amplification);
        }
      }
    }
  }

  _mesh.comm().max(maximum_amplification);
  if (_component == 0 && _mesh.comm().rank() == 0)
    mooseInfoRepeated(
        "Maximum element-linear stress recovery weight amplification sum(abs(w_q)) = ",
        maximum_amplification);

  _recovery_offsets.copyToDevice();
  _recovery_weights.copyToDevice();
  _recovery_initialized = true;
}
