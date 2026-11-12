//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosMaterial.h"

/**
 * Computes the total deformation gradient as a Kokkos rank-two material property.
 *
 * With stabilize_strain, it also computes the total-mode F-bar deformation gradient
 * B = (det(M) / det(U))^(1/3) U, where U is the physical deformation gradient and M is the average
 * of U over the true reference volume of the element that owns the evaluation point.
 */
class KokkosComputeDeformationGradient : public Moose::Kokkos::Material
{
public:
  static InputParameters validParams();

  KokkosComputeDeformationGradient(const InputParameters & parameters);

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const;

private:
  /**
   * Average the deformation gradient over the volume quadrature of the element that owns datum.
   * The volume geometry and the displacement gradients are computed from the mesh and the
   * solution degrees of freedom, so face, neighbor, and ghost-neighbor evaluations all use the
   * owning element's true volume rather than the face quadrature of the current context.
   */
  KOKKOS_FUNCTION Moose::Kokkos::Real33 volumeAverageDeformationGradient(Datum & datum) const;

  const unsigned int _ndisp;
  const bool _stabilize_strain;
  const Moose::Kokkos::VariableGradient _grad_displacements;
  /// Physical deformation gradient U = I + grad(u)
  Moose::Kokkos::MaterialProperty<Real, 2> _physical_deformation_gradient;
  /// F-bar deformation gradient B, declared only with stabilize_strain
  Moose::Kokkos::MaterialProperty<Real, 2> _stabilized_deformation_gradient;
  /// Volume average M of the physical deformation gradient, declared only with stabilize_strain
  Moose::Kokkos::MaterialProperty<Real, 2> _average_deformation_gradient;
};

template <typename Derived>
KOKKOS_FUNCTION void
KokkosComputeDeformationGradient::computeQpProperties(const unsigned int qp, Datum & datum) const
{
  auto deformation_gradient = _physical_deformation_gradient(datum, qp);
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      deformation_gradient(i, j) = i == j ? 1.0 : 0.0;

  for (unsigned int i = 0; i < _ndisp; ++i)
    for (unsigned int j = 0; j < _ndisp; ++j)
      deformation_gradient(i, j) += _grad_displacements(datum, qp, i)(j);

  if (!_stabilize_strain)
    return;

  Moose::Kokkos::Real33 U;
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      U(i, j) = deformation_gradient(i, j);

  const auto M = volumeAverageDeformationGradient(datum);
  const Real gamma = ::Kokkos::cbrt(M.determinant() / U.determinant());

  auto average = _average_deformation_gradient(datum, qp);
  auto stabilized = _stabilized_deformation_gradient(datum, qp);
  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
    {
      average(i, j) = M(i, j);
      stabilized(i, j) = gamma * U(i, j);
    }
}

KOKKOS_FUNCTION inline Moose::Kokkos::Real33
KokkosComputeDeformationGradient::volumeAverageDeformationGradient(Datum & datum) const
{
  Datum volume(datum.elemID(),
               libMesh::invalid_uint,
               libMesh::DofObject::invalid_id,
               datum.assembly(),
               kokkosSystems(),
               true);
  const auto & elem = volume.elem();
  const auto & variable = _grad_displacements.variable();

  Moose::Kokkos::Real33 grad_u_integral;
  Real measure = 0.0;
  for (unsigned int qp = 0; qp < volume.n_qps(); ++qp)
  {
    const Real JxW = volume.JxW(qp);
    measure += JxW;
    for (unsigned int i = 0; i < _ndisp; ++i)
    {
      const auto & system = volume.system(variable.sys(i));
      const auto var = variable.var(i);
      const auto fe = system.getFETypeID(var);
      const auto & grad_phi = volume.assembly().getGradPhi(elem.subdomain, elem.type, fe);

      Moose::Kokkos::Real3 grad_u;
      for (unsigned int k = 0; k < volume.assembly().getNumDofs(elem.type, fe); ++k)
        grad_u +=
            system.getVectorDofValue(system.getElemLocalDofIndex(elem.id, k, var), variable.tag()) *
            grad_phi(k, qp);
      grad_u = volume.J(qp) * grad_u;

      for (unsigned int j = 0; j < _ndisp; ++j)
        grad_u_integral(i, j) += JxW * grad_u(j);
    }
  }

  Moose::Kokkos::Real33 M;
  M.identity();
  for (unsigned int i = 0; i < _ndisp; ++i)
    for (unsigned int j = 0; j < _ndisp; ++j)
      M(i, j) += grad_u_integral(i, j) / measure;
  return M;
}
