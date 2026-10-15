//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceMaterial.h"
#include "NEML2OutputInterface.h"
#include "MathUtils.h"

/**
 * Copies a symmetric or full rank-two or rank-four NEML2 output into a Kokkos material property.
 *
 * The Kokkos material-property rank is part of the MaterialProperty<Real, rank> type, so it must
 * be fixed at compile time for device code. The interface and neighbor template parameters likewise
 * select the appropriate material dispatcher and property storage at compile time. The public
 * aliases below provide factory-registerable names for these specializations without duplicating
 * their implementation.
 */
template <unsigned int rank, bool full = false, bool interface = false, bool neighbor = false>
class NEML2ToKokkosMaterialProperty
  : public std::conditional_t<interface, Moose::Kokkos::InterfaceMaterial, Moose::Kokkos::Material>,
    public NEML2OutputInterface
{
  static_assert(rank == 2 || rank == 4, "Only rank-two and rank-four properties are supported.");

public:
  using MaterialBase =
      std::conditional_t<interface, Moose::Kokkos::InterfaceMaterial, Moose::Kokkos::Material>;

  static InputParameters validParams();

  NEML2ToKokkosMaterialProperty(const InputParameters & params);
  NEML2ToKokkosMaterialProperty(const NEML2ToKokkosMaterialProperty & object);

  void meshChanged() override;
  void computeProperties() override;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const unsigned int qp, Datum & datum) const;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           const unsigned int primary_qp,
                                           const unsigned int neighbor_storage_qp) const;

private:
  KOKKOS_FUNCTION static unsigned int mandelIndex(unsigned int i, unsigned int j);
  KOKKOS_FUNCTION static Real mandelFactor(unsigned int index);
  KOKKOS_FUNCTION void writeOutputToProperty(const dof_id_type batch_index,
                                             Datum & datum,
                                             unsigned int storage_qp) const;

  Moose::Kokkos::MaterialProperty<Real, rank> _prop;
  Moose::Kokkos::Array<Real> _staged_output;
  Moose::Kokkos::Array<dof_id_type> _batch_offset;
  bool _batch_mapping_outdated = true;
  bool _broadcast_output = false;
};

using NEML2ToKokkosRankTwoMaterialProperty = NEML2ToKokkosMaterialProperty<2>;
using NEML2ToKokkosRankFourMaterialProperty = NEML2ToKokkosMaterialProperty<4>;
using NEML2ToKokkosFullRankTwoMaterialProperty = NEML2ToKokkosMaterialProperty<2, true>;
using NEML2ToKokkosFullRankFourMaterialProperty = NEML2ToKokkosMaterialProperty<4, true>;
using NEML2ToKokkosSideRankTwoMaterialProperty = NEML2ToKokkosMaterialProperty<2, false, true>;
using NEML2ToKokkosSideRankFourMaterialProperty = NEML2ToKokkosMaterialProperty<4, false, true>;
using NEML2ToKokkosNeighborRankTwoMaterialProperty =
    NEML2ToKokkosMaterialProperty<2, false, true, true>;
using NEML2ToKokkosNeighborRankFourMaterialProperty =
    NEML2ToKokkosMaterialProperty<4, false, true, true>;

template <unsigned int rank, bool full, bool interface, bool neighbor>
KOKKOS_FUNCTION unsigned int
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::mandelIndex(const unsigned int i,
                                                                            const unsigned int j)
{
  if (i == j)
    return i;
  if ((i == 1 && j == 2) || (i == 2 && j == 1))
    return 3;
  if ((i == 0 && j == 2) || (i == 2 && j == 0))
    return 4;
  return 5;
}

template <unsigned int rank, bool full, bool interface, bool neighbor>
KOKKOS_FUNCTION Real
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::mandelFactor(
    const unsigned int index)
{
  return index < 3 ? 1.0 : MathUtils::sqrt2;
}

template <unsigned int rank, bool full, bool interface, bool neighbor>
KOKKOS_FUNCTION void
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::writeOutputToProperty(
    const dof_id_type batch_index, Datum & datum, const unsigned int storage_qp) const
{
  auto prop = _prop(datum, storage_qp);
  const unsigned int dim = this->kokkosAssembly().getDimension();
  for (unsigned int i = 0; i < dim; ++i)
    for (unsigned int j = 0; j < dim; ++j)
    {
      if constexpr (full)
      {
        if constexpr (rank == 2)
          prop(i, j) = batch_index == libMesh::DofObject::invalid_id
                           ? 0.0
                           : _staged_output[batch_index * 9 + 3 * i + j];
        else
          for (unsigned int k = 0; k < dim; ++k)
            for (unsigned int l = 0; l < dim; ++l)
              prop(i, j, k, l) =
                  batch_index == libMesh::DofObject::invalid_id
                      ? 0.0
                      : _staged_output[batch_index * 81 + 27 * i + 9 * j + 3 * k + l];
      }
      else
      {
        const auto a = mandelIndex(i, j);
        if constexpr (rank == 2)
          prop(i, j) = batch_index == libMesh::DofObject::invalid_id
                           ? 0.0
                           : _staged_output[batch_index * 6 + a] / mandelFactor(a);
        else
          for (unsigned int k = 0; k < dim; ++k)
            for (unsigned int l = 0; l < dim; ++l)
            {
              const auto b = mandelIndex(k, l);
              prop(i, j, k, l) = batch_index == libMesh::DofObject::invalid_id
                                     ? 0.0
                                     : _staged_output[batch_index * 36 + 6 * a + b] /
                                           (mandelFactor(a) * mandelFactor(b));
            }
      }
    }
}

template <unsigned int rank, bool full, bool interface, bool neighbor>
template <typename Derived>
KOKKOS_FUNCTION void
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::computeQpProperties(
    const unsigned int qp, Datum & datum) const
{
  const auto storage_qp = qp;
  const auto batch_index =
      _broadcast_output ? 0 : _batch_offset[datum.elemID()] + static_cast<dof_id_type>(storage_qp);
  writeOutputToProperty(batch_index, datum, storage_qp);
}

template <unsigned int rank, bool full, bool interface, bool neighbor>
template <typename Derived>
KOKKOS_FUNCTION void
NEML2ToKokkosMaterialProperty<rank, full, interface, neighbor>::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord & face,
    Datum & elem_datum,
    Datum & neighbor_datum,
    const unsigned int primary_qp,
    const unsigned int neighbor_storage_qp) const
{
  const auto offset = _batch_offset[face.face_index];
  const auto batch_index = offset == libMesh::DofObject::invalid_id ? libMesh::DofObject::invalid_id
                           : _broadcast_output                      ? 0
                                                                    : offset + primary_qp;
  if constexpr (neighbor)
    // MOOSEQuantityToNEML2 appends neighbor-side data while traversing the primary QP loop.
    // primary_qp selects the NEML2 batch entry; neighbor_storage_qp selects its destination in
    // neighbor material storage. Their correspondence must be a bijection.
    writeOutputToProperty(batch_index, neighbor_datum, neighbor_storage_qp);
  else
    writeOutputToProperty(batch_index, elem_datum, primary_qp);
}
