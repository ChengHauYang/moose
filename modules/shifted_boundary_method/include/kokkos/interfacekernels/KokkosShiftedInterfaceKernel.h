//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details

#pragma once

#include "KokkosInterfaceKernel.h"

class BoundaryShortestDistanceToSurface;

namespace Moose::Kokkos
{

/** Generic Kokkos interface base providing static shifted-boundary geometry. */
class ShiftedInterfaceKernel : public InterfaceKernel
{
public:
  static InputParameters validParams();

  ShiftedInterfaceKernel(const InputParameters & parameters);
  ShiftedInterfaceKernel(const ShiftedInterfaceKernel & object);

  virtual void computeResidual() override;
  virtual void computeJacobian() override;

protected:
  KOKKOS_FUNCTION Real3 shift(const AssemblyDatum & datum, unsigned int qp) const
  {
    return _shift[_geometry_offset(datum.side(), datum.elemID()) + qp];
  }

  KOKKOS_FUNCTION Real3 trueNormal(const AssemblyDatum & datum, unsigned int qp) const
  {
    return _true_normal[_geometry_offset(datum.side(), datum.elemID()) + qp];
  }

  KOKKOS_FUNCTION Real areaCorrection(AssemblyDatum & datum, unsigned int qp) const
  {
    return trueNormal(datum, qp).dot_product(datum.normals(qp));
  }

  KOKKOS_FUNCTION Real shiftedValue(const VariableValue & value,
                                     const VariableGradient & gradient,
                                     AssemblyDatum & datum,
                                     const Real3 & distance,
                                     unsigned int qp) const
  {
    return value(datum, qp) + gradient(datum, qp).dot_product(distance);
  }

  KOKKOS_FUNCTION Real shiftedTest(const VariableTestValue & test,
                                    const VariableTestGradient & gradient,
                                    AssemblyDatum & datum,
                                    const Real3 & distance,
                                    unsigned int i,
                                    unsigned int qp) const
  {
    return test(datum, i, qp) + gradient(datum, i, qp).dot_product(distance);
  }

  KOKKOS_FUNCTION Real shiftedShape(const VariablePhiValue & phi,
                                     const VariablePhiGradient & gradient,
                                     AssemblyDatum & datum,
                                     const Real3 & distance,
                                     unsigned int j,
                                     unsigned int qp) const
  {
    return phi(datum, j, qp) + gradient(datum, j, qp).dot_product(distance);
  }

private:
  void initializeShiftedGeometry();

  const BoundaryShortestDistanceToSurface & _distance_uo;
  bool _geometry_initialized = false;
  Array2D<dof_id_type> _geometry_offset;
  Array<Real3> _shift;
  Array<Real3> _true_normal;
};

} // namespace Moose::Kokkos
