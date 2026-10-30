//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeDeformationGradient.h"
#include "Assembly.h"
#include "MooseVariableFE.h"
#include "SystemBase.h"

#include "libmesh/dof_map.h"

registerMooseObject("SolidMechanicsApp", ComputeDeformationGradient);

InputParameters
ComputeDeformationGradient::validParams()
{
  InputParameters params = Material::validParams();
  params.addClassDescription(
      "Computes the total deformation gradient, optionally with total-mode F-bar stabilization "
      "based on the true element-volume average.");
  params.addRequiredCoupledVar("displacements", "Displacement variables in Cartesian order.");
  params.addParam<MaterialPropertyName>(
      "deformation_gradient",
      "deformation_gradient",
      "Name of the deformation gradient property. With stabilize_strain, this is the total-mode "
      "F-bar deformation gradient.");
  params.addParam<bool>(
      "stabilize_strain",
      false,
      "Also compute the total-mode F-bar deformation gradient from the average of the physical "
      "deformation gradient over the true reference volume of the owning element.");
  params.addParam<MaterialPropertyName>(
      "physical_deformation_gradient",
      "physical_deformation_gradient",
      "Name of the physical deformation gradient property used with stabilize_strain.");
  params.addParam<MaterialPropertyName>(
      "average_deformation_gradient",
      "average_deformation_gradient",
      "Name of the element-volume average of the physical deformation gradient used with "
      "stabilize_strain.");
  return params;
}

ComputeDeformationGradient::ComputeDeformationGradient(const InputParameters & parameters)
  : Material(parameters),
    _ndisp(coupledComponents("displacements")),
    _stabilize_strain(getParam<bool>("stabilize_strain")),
    _grad_disp(coupledGradients("displacements")),
    _physical_deformation_gradient(declareProperty<RankTwoTensor>(getParam<MaterialPropertyName>(
        _stabilize_strain ? "physical_deformation_gradient" : "deformation_gradient"))),
    _stabilized_deformation_gradient(
        _stabilize_strain ? &declareProperty<RankTwoTensor>(
                                getParam<MaterialPropertyName>("deformation_gradient"))
                          : nullptr),
    _average_deformation_gradient(
        _stabilize_strain ? &declareProperty<RankTwoTensor>(
                                getParam<MaterialPropertyName>("average_deformation_gradient"))
                          : nullptr)
{
  if (_ndisp != _mesh.dimension())
    paramError("displacements",
               "The number of displacement variables must match the mesh dimension.");

  for (const auto i : make_range(_ndisp))
    _disp_vars.push_back(getVar("displacements", i));

  if (_stabilize_strain)
  {
    if (_ndisp != 3)
      paramError("stabilize_strain",
                 "F-bar stabilization requires three-dimensional displacements.");
    for (const auto i : make_range(_ndisp))
      if (_disp_vars[i]->feType() != _disp_vars[0]->feType())
        paramError("displacements",
                   "F-bar stabilization requires all displacements to share one finite element "
                   "type.");
  }
  else
    for (const auto & param : {"physical_deformation_gradient", "average_deformation_gradient"})
      if (isParamSetByUser(param))
        paramError(param, "This parameter requires stabilize_strain = true.");
}

void
ComputeDeformationGradient::initialSetup()
{
  if (getBlockCoordSystem() != Moose::COORD_XYZ)
    mooseError("ComputeDeformationGradient only supports Cartesian coordinates.");
}

void
ComputeDeformationGradient::computeProperties()
{
  if (_stabilize_strain)
    _current_average = volumeAverageDeformationGradient();

  Material::computeProperties();
}

void
ComputeDeformationGradient::computeQpProperties()
{
  auto & U = _physical_deformation_gradient[_qp];
  U = RankTwoTensor::Identity();
  for (const auto i : make_range(_ndisp))
    for (const auto j : make_range(_ndisp))
      U(i, j) += (*_grad_disp[i])[_qp](j);

  if (!_stabilize_strain)
    return;

  (*_average_deformation_gradient)[_qp] = _current_average;
  (*_stabilized_deformation_gradient)[_qp] = std::cbrt(_current_average.det() / U.det()) * U;
}

RankTwoTensor
ComputeDeformationGradient::volumeAverageDeformationGradient()
{
  // The volume rule of the current assembly context is reused for the owning element. Elements on
  // both sides of an interface must therefore share one volume quadrature rule.
  if (!_volume_fe)
  {
    const auto & volume_rule = *_assembly.qRule();
    _volume_qrule =
        libMesh::QBase::build(volume_rule.type(), _mesh.dimension(), volume_rule.get_order());
    _volume_fe = libMesh::FEBase::build(_mesh.dimension(), _disp_vars[0]->feType());
    _volume_fe->attach_quadrature_rule(_volume_qrule.get());
    _volume_fe->get_dphi();
    _volume_fe->get_JxW();
  }

  _volume_fe->reinit(_current_elem);
  const auto & dphi = _volume_fe->get_dphi();
  const auto & JxW = _volume_fe->get_JxW();

  Real measure = 0;
  for (const auto qp : index_range(JxW))
    measure += JxW[qp];

  RankTwoTensor average = RankTwoTensor::Identity();
  std::vector<dof_id_type> dof_indices;
  for (const auto i : make_range(_ndisp))
  {
    // Read the solution directly so the average is also available on ghosted neighbor elements.
    auto & sys = _disp_vars[i]->sys();
    const auto & solution = *sys.currentSolution();
    sys.dofMap().dof_indices(_current_elem, dof_indices, _disp_vars[i]->number());
    mooseAssert(dof_indices.size() == dphi.size(), "Shape function and DOF counts must match.");

    for (const auto qp : index_range(JxW))
    {
      RealVectorValue grad_u;
      for (const auto k : index_range(dof_indices))
        grad_u += solution(dof_indices[k]) * dphi[k][qp];
      for (const auto j : make_range(_ndisp))
        average(i, j) += JxW[qp] * grad_u(j) / measure;
    }
  }

  return average;
}
