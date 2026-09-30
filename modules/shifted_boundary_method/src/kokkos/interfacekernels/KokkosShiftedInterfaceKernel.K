//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#include "KokkosShiftedInterfaceKernel.h"

#include "BoundaryShortestDistanceToSurface.h"
#include "MooseMesh.h"

namespace Moose::Kokkos
{

InputParameters
ShiftedInterfaceKernel::validParams()
{
  auto params = InterfaceKernel::validParams();
  params.addRequiredParam<UserObjectName>(
      "sbm_distance_uo", "UserObject providing the static shifted-interface geometry.");
  return params;
}

ShiftedInterfaceKernel::ShiftedInterfaceKernel(const InputParameters & parameters)
  : InterfaceKernel(parameters),
    _distance_uo(getUserObject<BoundaryShortestDistanceToSurface>("sbm_distance_uo"))
{
}

ShiftedInterfaceKernel::ShiftedInterfaceKernel(const ShiftedInterfaceKernel & object)
  : InterfaceKernel(object),
    _distance_uo(object._distance_uo),
    _geometry_initialized(object._geometry_initialized),
    _geometry_offset(object._geometry_offset),
    _shift(object._shift),
    _true_normal(object._true_normal)
{
}

void
ShiftedInterfaceKernel::computeResidual()
{
  initializeShiftedGeometry();
  InterfaceKernel::computeResidual();
}

void
ShiftedInterfaceKernel::computeJacobian()
{
  initializeShiftedGeometry();
  InterfaceKernel::computeJacobian();
}

void
ShiftedInterfaceKernel::initializeShiftedGeometry()
{
  if (_geometry_initialized)
    return;

  const auto & faces = kokkosInterfaceFaces();
  std::vector<Real3> shift;
  std::vector<Real3> true_normal;
  dof_id_type n_qps = 0;
  for (const auto & face : faces)
    n_qps += face.n_qps;
  shift.reserve(n_qps);
  true_normal.reserve(n_qps);

  const auto & mesh = *this->_mesh.getKokkosMesh();
  std::map<ContiguousElementID, const Elem *> host_elements;
  for (const auto boundary : boundaryIDs())
    for (const auto elem_id : this->_mesh.getBoundaryActiveSemiLocalElemIds(boundary))
    {
      const auto * const elem = this->_mesh.elemPtr(elem_id);
      if (elem->processor_id() == processor_id())
        host_elements.emplace(mesh.getContiguousElementID(elem), elem);
    }

  for (const auto & face : faces)
  {
    const auto * const elem = libmesh_map_find(host_elements, face.elem);
    const BoundaryShortestDistanceToSurface::ElemSide elem_side(elem->id(), face.side);
    for (const auto qp : make_range(face.n_qps))
    {
      shift.emplace_back(_distance_uo.surrogateDistance(elem_side, qp));
      true_normal.emplace_back(_distance_uo.trueNormal(elem_side, qp));
    }
  }

  _geometry_offset.create(this->_mesh.getMaxSidesPerElem(),
                          mesh.getNumLocalAndPossiblyOneNeighborLayerGhostElements());
  _geometry_offset = libMesh::DofObject::invalid_id;
  for (const auto & face : faces)
    _geometry_offset(face.side, face.elem) = face.qp_map_offset;
  _geometry_offset.copyToDevice();

  _shift = shift;
  _true_normal = true_normal;
  _geometry_initialized = true;
}

} // namespace Moose::Kokkos
