//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "SubdomainInterceptedGenerator.h"
#include "Conversion.h"
#include "CastUniquePointer.h"
#include "MooseUtils.h"
#include "MooseMeshUtils.h"

#include "libmesh/elem.h"
#include "libmesh/fe.h"
#include "libmesh/fe_base.h"
#include "libmesh/fe_type.h"
#include "libmesh/quadrature_gauss.h"

registerMooseObject("MooseApp", SubdomainInterceptedGenerator);

InputParameters
SubdomainInterceptedGenerator::validParams()
{
  InputParameters params = MeshGenerator::validParams();
  params += FunctionParserUtils<false>::validParams();

  params.addRequiredParam<MeshGeneratorName>("input", "The mesh we want to modify");
  params.addRequiredParam<std::string>("function", "Function expression to evaluate");

  params.addRequiredParam<SubdomainID>("subdomain_id_inside", "ID for inside elements.");
  params.addRequiredParam<SubdomainID>("subdomain_id_outside", "ID for outside elements.");

  params.addParam<Real>("threshold", 0.0, "Threshold for inside/outside classification.");
  params.addRequiredParam<Real>("lambda", "Lambda for false intersection classification.");
  params.addRequiredParam<bool>("outer_boundary", "Flag for outer boundary handling.");
  params.addParam<bool>("multi_geo", false, "Do you have multiple geometries to do in-out test?");
  params.addParam<bool>("keep_inside_as_inside", false, "Keep inside as inside.");
  params.addParam<bool>("keep_outside_as_outside", false, "Keep outside as outside.");

  // Quadrature order used for active‑area estimation when an element straddles the interface
  params.addParam<int>("qrule_order", 6, "Quadrature order used to estimate the active area.");

  return params;
}

SubdomainInterceptedGenerator::SubdomainInterceptedGenerator(const InputParameters & parameters)
  : MeshGenerator(parameters),
    FunctionParserUtils<false>(parameters),
    _input(getMesh("input")),
    _parsed_function(std::make_shared<SymFunction>()),
    _subdomain_id_inside(getParam<SubdomainID>("subdomain_id_inside")),
    _subdomain_id_outside(getParam<SubdomainID>("subdomain_id_outside")),
    _threshold(getParam<Real>("threshold")),
    _lambda(getParam<Real>("lambda")),
    _outer_boundary(getParam<bool>("outer_boundary")),
    _multi_geo(getParam<bool>("multi_geo")),
    _keep_inside_as_inside(getParam<bool>("keep_inside_as_inside")),
    _keep_outside_as_outside(getParam<bool>("keep_outside_as_outside")),
    _qrule_order(getParam<int>("qrule_order"))
{

  setParserFeatureFlags(_parsed_function);

  if (_parsed_function->Parse(getParam<std::string>("function"), "x,y,z") >= 0)
    mooseError("Invalid function expression provided.");

  _func_params.resize(3);
}

/**
 * Helper: build an FEBase with a Gauss rule of order _qrule_order.
 */
static std::unique_ptr<libMesh::FEBase>
_buildFEB(const libMesh::Elem * elem, int qrule_order)
{
  libMesh::Order order = static_cast<libMesh::Order>(qrule_order);
  libMesh::FEType fe_type(elem->default_order(), LAGRANGE);
  std::unique_ptr<libMesh::FEBase> fe(libMesh::FEBase::build(elem->dim(), fe_type));
  libMesh::QGauss qrule(elem->dim(), order);
  fe->get_xyz(); // this is very important, otherwise the quadrature points are not
  // initialized
  fe->get_JxW();
  fe->attach_quadrature_rule(&qrule);
  fe->reinit(elem);
  return fe;
}

/**
 * Helper: compute the fraction of the element volume/area that satisfies a
 * Boolean predicate evaluated at quadrature points.
 */
static Real
_computeActiveRatio(const libMesh::Elem * elem,
                    int qrule_order,
                    const std::function<bool(const Point &)> & predicate)
{
  auto fe = _buildFEB(elem, qrule_order);
  const auto & JxW = fe->get_JxW();
  const auto & q_points = fe->get_xyz();

  Real active_measure = 0.0, total_measure = 0.0;
  for (unsigned int qp = 0; qp < q_points.size(); ++qp)
  {
    if (predicate(q_points[qp]))
      active_measure += JxW[qp];
    total_measure += JxW[qp];
  }
  mooseAssert(total_measure > 0.0, "Element with zero measure?");
  return active_measure / total_measure;
}

std::unique_ptr<libMesh::MeshBase>
SubdomainInterceptedGenerator::generate()
{
  // Take ownership of the input mesh (already cloned by getMesh()).
  std::unique_ptr<libMesh::MeshBase> mesh = std::move(_input);

  for (const auto & elem : mesh->active_local_element_ptr_range())
  {
    // (a) Skip elements that have already been explicitly assigned by a
    //       previous geometry in a multi‑geometry workflow.
    if (_multi_geo)
    {
      if (elem->subdomain_id() == _subdomain_id_inside && _keep_inside_as_inside)
        continue;
      else if (elem->subdomain_id() == _subdomain_id_outside && _keep_outside_as_outside)
        continue;
    }

    // (b) Evaluate the distance function at the element nodes and capture
    //       the extrema.
    Real min_phi = std::numeric_limits<Real>::max();
    Real max_phi = std::numeric_limits<Real>::lowest();

    for (unsigned int n = 0; n < elem->n_nodes(); ++n)
    {
      const Point & X = elem->point(n);
      _func_params = {X(0), X(1), X(2)};
      Real phi = evaluate(_parsed_function);
      min_phi = std::min(min_phi, phi);
      max_phi = std::max(max_phi, phi);
    }

    // std::cout << "min_phi = " << min_phi << std::endl;
    // std::cout << "max_phi = " << max_phi << std::endl;

    // (c) Trivial inside / outside if all nodes are on the same side.
    if (max_phi < _threshold)
    {
      elem->subdomain_id() = _outer_boundary ? _subdomain_id_inside : _subdomain_id_outside;
      continue;
    }
    else if (min_phi > _threshold)
    {
      elem->subdomain_id() = _outer_boundary ? _subdomain_id_outside : _subdomain_id_inside;
      continue;
    }

    // (d) Element straddles the interface – estimate the active‑area ratio
    //       with Gaussian quadrature.
    auto is_active = [&](const Point & p)
    {
      _func_params = {p(0), p(1), p(2)};
      Real phi = evaluate(_parsed_function);
      return (_outer_boundary && phi < _threshold) || (!_outer_boundary && phi > _threshold);
    };

    Real ratio_active = _computeActiveRatio(elem, _qrule_order, is_active);

    // (e) Decide false / true interception based on _lambda.
    if (_lambda == 0.0)
      elem->subdomain_id() = _subdomain_id_inside;
    else if (_lambda == 1.0)
      elem->subdomain_id() = _subdomain_id_outside;
    else
    {
      bool is_false_intercepted = ((1.0 - ratio_active) > _lambda);

      if (is_false_intercepted)
        elem->subdomain_id() = _subdomain_id_outside;
      else
        elem->subdomain_id() = _subdomain_id_inside;
    }
  }

  // Signal that the mesh has been modified and needs preparation.
  mesh->set_isnt_prepared();
  return mesh;
}
