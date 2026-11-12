//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

// initQpStatefulProperties() and computeQpProperties() are intentionally hidden
// but some compilers generate ugly warnings

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

#include "KokkosMaterial.h"
#include "KokkosInterfaceDatum.h"
#include "KokkosVariableValue.h"

namespace Moose::Kokkos
{

/**
 * The base class for a user to derive their own Kokkos interface materials.
 *
 * An interface material evaluates a constitutive law on the two sides of a finite element interface
 * face. It runs once per oriented interface face of its boundary restriction, and shares the face
 * ordering with the interface kernels of the same boundaries, so that a local face index identifies
 * the same face for both. Only the rank owning the primary element of a face evaluates that face,
 * which makes the primary element owner the single writer of the properties of that face. The
 * interface kernels of the same boundaries read those properties from both of their sides.
 *
 * The user should define initQpStatefulProperties() and computeQpProperties() as inlined public
 * methods in their derived class (not virtual override). The signature of computeQpProperties()
 * expected to be defined in the derived class is as follows:
 *
 * @tparam Derived The object type
 * @param face The oriented interface face record
 * @param elem_datum The Datum object of the primary side of the face
 * @param neighbor_datum The Datum object of the neighbor side of the face
 * @param qp The primary-side local quadrature point index
 * @param neighbor_qp The neighbor-side local quadrature point index at the same physical location
 *
 * template <typename Derived>
 * KOKKOS_FUNCTION void computeQpProperties(const InterfaceFaceRecord & face,
 *                                          Datum & elem_datum,
 *                                          Datum & neighbor_datum,
 *                                          const unsigned int qp,
 *                                          const unsigned int neighbor_qp) const;
 *
 * The signature of initQpStatefulProperties() can be found in the code below, and its definition in
 * the derived class is optional. If it is defined in the derived class, it will hide the default
 * definition in the base class.
 */
class InterfaceMaterial : public Material
{
public:
  static InputParameters validParams();

  /**
   * Constructor
   */
  InterfaceMaterial(const InputParameters & parameters);

  /**
   * Copy constructor for parallel dispatch
   */
  InterfaceMaterial(const InterfaceMaterial & object);

  /**
   * Dispatch stateful interface material property initialization
   */
  void initStatefulProperties(unsigned int) override;

  /**
   * Dispatch interface material property evaluation
   */
  void computeProperties() override;

  bool isBoundaryMaterial() const override { return true; }

  /**
   * Default methods to prevent compile errors even when these methods were not defined in the
   * derived class
   */
  ///@{
  /**
   * Initialize stateful interface material properties on a quadrature point
   * @tparam Derived The object type
   * @param face The oriented interface face record
   * @param elem_datum The Datum object of the primary side of the face
   * @param neighbor_datum The Datum object of the neighbor side of the face
   * @param qp The primary-side local quadrature point index
   * @param neighbor_qp The neighbor-side local quadrature point index at the same physical location
   */
  template <typename Derived>
  KOKKOS_FUNCTION void initQpStatefulProperties(const InterfaceFaceRecord & /* face */,
                                                Datum & /* elem_datum */,
                                                Datum & /* neighbor_datum */,
                                                const unsigned int /* qp */,
                                                const unsigned int /* neighbor_qp */) const
  {
    ::Kokkos::abort(
        "Default initQpStatefulProperties() should never be called. Make sure you properly "
        "redefined this method in your class without typos.");
  }
  ///@}

  /**
   * Function used to check if users have overriden the hook method, whose calculation can be
   * skipped when not overriden
   * @returns The function pointer of the default hook method
   */
  ///@{
  template <typename Derived>
  static auto defaultInitStateful()
  {
    return &InterfaceMaterial::initQpStatefulProperties<Derived>;
  }
  ///@}

  /**
   * The parallel computation entry functions called by Kokkos
   */
  ///@{
  struct InterfaceInit
  {
  };
  struct InterfaceCompute
  {
  };
  ///@}

  /**
   * Kokkos functor dispatchers
   */
  ///@{
  template <typename Derived>
  KOKKOS_FUNCTION void
  operator()(InterfaceInit, const ThreadID tid, const Derived & material) const;
  template <typename Derived>
  KOKKOS_FUNCTION void
  operator()(InterfaceCompute, const ThreadID tid, const Derived & material) const;
  ///@}

protected:
  /// Value of the primary-side interface variable
  const VariableValue _variable_value;
  /// Value of the neighbor-side interface variable
  const VariableValue _neighbor_variable_value;

  /// Get the number of oriented interface faces this material evaluates on
  KOKKOS_FUNCTION dof_id_type numKokkosInterfaceFaces() const { return _interface_faces.size(); }

  /**
   * Get an oriented interface face record
   * @param tid The thread ID
   */
  KOKKOS_FUNCTION const InterfaceFaceRecord & kokkosInterfaceFace(const ThreadID tid) const
  {
    return _interface_faces[tid];
  }

  /// Get the shared oriented interface face records on the host.
  const Array<InterfaceFaceRecord> & kokkosInterfaceFaces() const { return _interface_faces; }

  /**
   * Get the neighbor-side quadrature point index coinciding with a primary-side quadrature point
   * @param face The oriented interface face record
   * @param qp The primary-side local quadrature point index
   */
  KOKKOS_FUNCTION unsigned int kokkosInterfaceNeighborQp(const InterfaceFaceRecord & face,
                                                         const unsigned int qp) const
  {
    return _neighbor_qp_map[face.qp_map_offset + qp];
  }

  /**
   * Shallow handles into the face arrays owned by the FEProblemBase interface face cache. They
   * refer to the same device memory as the interface kernels of the same boundaries, so that a
   * local face index identifies the same face for both.
   */
  Array<InterfaceFaceRecord> _interface_faces;
  /// Neighbor-side quadrature point index for each primary-side quadrature point.
  Array<unsigned int> _neighbor_qp_map;

private:
  void acquireInterfaceFaces();
};

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceMaterial::operator()(InterfaceInit, const ThreadID tid, const Derived & material) const
{
  const auto & face = _interface_faces[tid];

  auto elem_datum = makePrimaryInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());
  auto neighbor_datum = makeNeighborInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());

  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
    material.template initQpStatefulProperties<Derived>(
        face, elem_datum, neighbor_datum, qp, _neighbor_qp_map[face.qp_map_offset + qp]);
}

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceMaterial::operator()(InterfaceCompute, const ThreadID tid, const Derived & material) const
{
  const auto & face = _interface_faces[tid];

  auto elem_datum = makePrimaryInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());
  auto neighbor_datum = makeNeighborInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());

  for (unsigned int qp = 0; qp < face.n_qps; ++qp)
    material.template computeQpProperties<Derived>(
        face, elem_datum, neighbor_datum, qp, _neighbor_qp_map[face.qp_map_offset + qp]);
}

} // namespace Moose::Kokkos
