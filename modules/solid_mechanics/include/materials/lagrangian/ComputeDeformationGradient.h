//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "Material.h"
#include "RankTwoTensor.h"

#include "libmesh/fe_base.h"
#include "libmesh/quadrature.h"

/**
 * Computes the total deformation gradient in Cartesian coordinates.
 *
 * With stabilize_strain, it also computes the total-mode F-bar deformation gradient
 * B = (det(M) / det(U))^(1/3) U, where U is the physical deformation gradient and M is the average
 * of U over the true reference volume of the element that owns the evaluation point. On element
 * faces and neighbor faces, M is integrated with a separate volume finite element reinitialized on
 * the owning element, so it never uses the face quadrature of the current context.
 */
class ComputeDeformationGradient : public Material
{
public:
  static InputParameters validParams();

  ComputeDeformationGradient(const InputParameters & parameters);

  virtual void initialSetup() override;

protected:
  virtual void computeProperties() override;
  virtual void computeQpProperties() override;

  /// Average the physical deformation gradient over the volume quadrature of _current_elem
  RankTwoTensor volumeAverageDeformationGradient();

  const unsigned int _ndisp;
  const bool _stabilize_strain;
  std::vector<const VariableGradient *> _grad_disp;
  std::vector<MooseVariable *> _disp_vars;

  /// Physical deformation gradient U = I + grad(u)
  MaterialProperty<RankTwoTensor> & _physical_deformation_gradient;
  /// F-bar deformation gradient B, declared only with stabilize_strain
  MaterialProperty<RankTwoTensor> * _stabilized_deformation_gradient;
  /// Volume average M of the physical deformation gradient, declared only with stabilize_strain
  MaterialProperty<RankTwoTensor> * _average_deformation_gradient;

  /// Volume quadrature and finite element used to integrate M on the owning element
  ///@{
  std::unique_ptr<libMesh::QBase> _volume_qrule;
  std::unique_ptr<libMesh::FEBase> _volume_fe;
  ///@}

  /// M for the element currently being evaluated
  RankTwoTensor _current_average;
};
