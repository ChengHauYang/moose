//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosADInterfaceKernel.h"

class BoundaryShortestDistanceToSurface;

namespace Moose::Kokkos
{

/// Generic AD Kokkos interface base providing static surrogate-interface mapping data.
class ADShiftedInterfaceKernel : public ADInterfaceKernel
{
public:
  static InputParameters validParams();

  ADShiftedInterfaceKernel(const InputParameters & parameters);
  ADShiftedInterfaceKernel(const ADShiftedInterfaceKernel & object);

  void computeResidual() override;
  void computeJacobian() override;
  void computeResidualAndJacobian() override;

protected:
  KOKKOS_FUNCTION Real3 surrogateDistanceVector(const AssemblyDatum & datum, unsigned int qp) const
  {
    return _surrogate_distance_vectors[_qp_offsets(datum.side(), datum.elemID()) + qp];
  }

  KOKKOS_FUNCTION Real3 trueNormal(const AssemblyDatum & datum, unsigned int qp) const
  {
    return _true_normals[_qp_offsets(datum.side(), datum.elemID()) + qp];
  }

  KOKKOS_FUNCTION Real areaCorrection(AssemblyDatum & datum, unsigned int qp) const
  {
    return trueNormal(datum, qp).dot_product(datum.normals(qp));
  }

private:
  void initializeSurrogateInterfaceMapping();
  void ensureSurrogateInterfaceMappingReady() const;

  const BoundaryShortestDistanceToSurface & _sbm_distance_uo;
  Array2D<dof_id_type> _qp_offsets;
  Array<Real3> _surrogate_distance_vectors;
  Array<Real3> _true_normals;
};

} // namespace Moose::Kokkos
