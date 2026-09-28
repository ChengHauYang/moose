//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosInterfaceKernelBase.h"

#include "FEProblemBase.h"
#include "MooseMesh.h"
#include "libmesh/elem.h"

namespace Moose::Kokkos
{

InputParameters
InterfaceKernelBase::validParams()
{
  auto params = ResidualObject::validParams();
  params += BoundaryRestrictableRequired::validParams();
  params += MaterialPropertyInterface::validParams();

  params.addRequiredCoupledVar("neighbor_var", "The variable on the other side of the interface.");
  params.registerBase("InterfaceKernel");
  params.registerSystemAttributeName("InterfaceKernel");
  params.addRelationshipManager("ElementSideNeighborLayers",
                                Moose::RelationshipManagerType::GEOMETRIC |
                                    Moose::RelationshipManagerType::ALGEBRAIC |
                                    Moose::RelationshipManagerType::COUPLING);

  return params;
}

InterfaceKernelBase::InterfaceKernelBase(const InputParameters & parameters)
  : ResidualObject(parameters, Moose::VarFieldType::VAR_FIELD_STANDARD),
    BoundaryRestrictableRequired(this, false),
    CoupleableMooseVariableDependencyIntermediateInterface(this, false),
    MaterialPropertyInterface(this, Moose::EMPTY_BLOCK_IDS, boundaryIDs()),
    _neighbor_var(*getVarHelper<MooseVariableFE<Real>>("neighbor_var", 0)),
    _kokkos_neighbor_var(_neighbor_var)
{
  addMooseVariableDependency(&_var);
  addMooseVariableDependency(&_neighbor_var);

  _fe_problem.addKokkosMeshInitializationHook(
      std::bind(&InterfaceKernelBase::initializeInterfaceFaces, this));
}

InterfaceKernelBase::InterfaceKernelBase(const InterfaceKernelBase & object)
  : ResidualObject(object),
    BoundaryRestrictableRequired(object, FunctorCopy()),
    CoupleableMooseVariableDependencyIntermediateInterface(object, FunctorCopy()),
    MaterialPropertyInterface(object, FunctorCopy()),
    _neighbor_var(object._neighbor_var),
    _kokkos_neighbor_var(object._kokkos_neighbor_var),
    _interface_faces(object._interface_faces),
    _neighbor_qp_map(object._neighbor_qp_map)
{
}

void
InterfaceKernelBase::initializeInterfaceFaces()
{
  std::vector<InterfaceFaceRecord> interface_faces;
  std::vector<unsigned int> neighbor_qp_map;
  std::set<Pair<ContiguousElementID, unsigned int>> visited_faces;
  const auto & mesh = _mesh;
  const auto & kokkos_mesh = *mesh.getKokkosMesh();
  const auto & assembly = _fe_problem.kokkosAssembly();

  for (const auto boundary : boundaryIDs())
    for (const auto elem_id : mesh.getBoundaryActiveSemiLocalElemIds(boundary))
    {
      const auto * const elem = mesh.elemPtr(elem_id);
      if (elem->processor_id() != processor_id())
        continue;

      const auto side = mesh.sideWithBoundaryID(elem, boundary);
      const Pair<ContiguousElementID, unsigned int> elem_side = {
          kokkos_mesh.getContiguousElementID(elem), side};
      if (!visited_faces.insert(elem_side).second)
        continue;

      const auto * const neighbor = elem->neighbor_ptr(side);
      if (!neighbor || neighbor == libMesh::remote_elem)
        paramError("boundary", "Kokkos interface faces must have a local or ghosted neighbor.");

      const auto neighbor_side = neighbor->which_neighbor_am_i(elem);
      if (neighbor_side == libMesh::invalid_uint)
        paramError("boundary", "Failed to identify the neighbor side of an interface face.");

      const auto neighbor_id = kokkos_mesh.getContiguousElementID(neighbor);
      const auto elem_info = kokkos_mesh.getElementInfo(elem_side.first);
      const auto neighbor_info = kokkos_mesh.getElementInfo(neighbor_id);
      const auto n_qps = assembly.getNumFaceQps(elem_info, side);
      const auto n_neighbor_qps = assembly.getNumFaceQps(neighbor_info, neighbor_side);
      if (n_qps != n_neighbor_qps)
        paramError("boundary",
                   "The two sides of a Kokkos interface face must have the same number of "
                   "quadrature points.");

      const auto qp_map_offset = neighbor_qp_map.size();
      std::vector<bool> neighbor_qp_used(n_neighbor_qps, false);
      // Both points come from FE maps of the same face. This scale admits mapping roundoff while
      // remaining well below the spacing between distinct quadrature points.
      const Real matching_tol = 100 * TOLERANCE * elem->hmax();
      const Real matching_tol_sq = matching_tol * matching_tol;

      for (const auto qp : make_range(n_qps))
      {
        Real3 elem_point;
        assembly.computePhysicalMap(elem_info, side, qp, nullptr, nullptr, &elem_point, nullptr);

        unsigned int matched_qp = libMesh::invalid_uint;
        for (const auto neighbor_qp : make_range(n_neighbor_qps))
        {
          if (neighbor_qp_used[neighbor_qp])
            continue;

          Real3 neighbor_point;
          assembly.computePhysicalMap(neighbor_info,
                                      neighbor_side,
                                      neighbor_qp,
                                      nullptr,
                                      nullptr,
                                      &neighbor_point,
                                      nullptr);
          const auto distance = (elem_point - neighbor_point).norm();
          if (distance * distance <= matching_tol_sq)
          {
            if (matched_qp != libMesh::invalid_uint)
              paramError("boundary",
                         "Kokkos interface quadrature point correspondence is ambiguous.");
            matched_qp = neighbor_qp;
          }
        }

        if (matched_qp == libMesh::invalid_uint)
          paramError("boundary",
                     "Failed to match a Kokkos interface quadrature point to the neighbor side.");

        neighbor_qp_used[matched_qp] = true;
        neighbor_qp_map.push_back(matched_qp);
      }

      interface_faces.push_back({elem_side.first,
                                 elem_side.second,
                                 neighbor_id,
                                 neighbor_side,
                                 qp_map_offset,
                                 n_qps});
    }

  _interface_faces = interface_faces;
  _neighbor_qp_map = neighbor_qp_map;
}

} // namespace Moose::Kokkos
