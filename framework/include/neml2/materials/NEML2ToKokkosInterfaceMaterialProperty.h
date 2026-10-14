//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceMaterial.h"
#include "NEML2OutputInterface.h"
#include "MathUtils.h"

/**
 * Copies a symmetric NEML2 rank-two or rank-four output into a primary or neighbor Kokkos interface
 * material property.
 */
template <unsigned int rank, bool neighbor>
class NEML2ToKokkosInterfaceMaterialProperty : public Moose::Kokkos::InterfaceMaterial,
                                               public NEML2OutputInterface
{
  static_assert(rank == 2 || rank == 4, "Only rank-two and rank-four properties are supported.");

public:
  static InputParameters validParams();

  NEML2ToKokkosInterfaceMaterialProperty(const InputParameters & params);
  NEML2ToKokkosInterfaceMaterialProperty(const NEML2ToKokkosInterfaceMaterialProperty & object);

  void meshChanged() override;
  void computeProperties() override;

  template <typename Derived>
  KOKKOS_FUNCTION void computeQpProperties(const Moose::Kokkos::InterfaceFaceRecord & face,
                                           Datum & elem_datum,
                                           Datum & neighbor_datum,
                                           unsigned int qp,
                                           unsigned int neighbor_qp) const;

private:
  KOKKOS_FUNCTION static unsigned int mandelIndex(unsigned int i, unsigned int j);
  KOKKOS_FUNCTION static Real mandelFactor(unsigned int index);

  Moose::Kokkos::MaterialProperty<Real, rank> _prop;
  Moose::Kokkos::Array<Real> _staged_output;
  Moose::Kokkos::Array<dof_id_type> _side_batch_offset;
  bool _batch_mapping_outdated = true;
  bool _broadcast_output = false;
  int64_t _batch_qp_stride = 0;
};

using NEML2ToKokkosSideRankTwoMaterialProperty = NEML2ToKokkosInterfaceMaterialProperty<2, false>;
using NEML2ToKokkosSideRankFourMaterialProperty = NEML2ToKokkosInterfaceMaterialProperty<4, false>;
using NEML2ToKokkosNeighborRankTwoMaterialProperty =
    NEML2ToKokkosInterfaceMaterialProperty<2, true>;
using NEML2ToKokkosNeighborRankFourMaterialProperty =
    NEML2ToKokkosInterfaceMaterialProperty<4, true>;

template <unsigned int rank, bool neighbor>
KOKKOS_FUNCTION unsigned int
NEML2ToKokkosInterfaceMaterialProperty<rank, neighbor>::mandelIndex(const unsigned int i,
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

template <unsigned int rank, bool neighbor>
KOKKOS_FUNCTION Real
NEML2ToKokkosInterfaceMaterialProperty<rank, neighbor>::mandelFactor(const unsigned int index)
{
  return index < 3 ? 1.0 : MathUtils::sqrt2;
}

template <unsigned int rank, bool neighbor>
template <typename Derived>
KOKKOS_FUNCTION void
NEML2ToKokkosInterfaceMaterialProperty<rank, neighbor>::computeQpProperties(
    const Moose::Kokkos::InterfaceFaceRecord & face,
    Datum & elem_datum,
    Datum & neighbor_datum,
    const unsigned int qp,
    const unsigned int neighbor_qp) const
{
  const auto batch = _broadcast_output
                         ? 0
                         : _side_batch_offset[face.qp_map_offset + (neighbor ? neighbor_qp : qp)] +
                               static_cast<dof_id_type>(neighbor ? neighbor_qp : qp);
  auto prop = neighbor ? _prop(neighbor_datum, neighbor_qp) : _prop(elem_datum, qp);
  const unsigned int dim = kokkosAssembly().getDimension();
  for (unsigned int i = 0; i < dim; ++i)
    for (unsigned int j = 0; j < dim; ++j)
      if constexpr (rank == 2)
      {
        const auto a = mandelIndex(i, j);
        prop(i, j) = _staged_output[batch * 6 + a] / mandelFactor(a);
      }
      else
        for (unsigned int k = 0; k < dim; ++k)
          for (unsigned int l = 0; l < dim; ++l)
          {
            const auto a = mandelIndex(i, j);
            const auto b = mandelIndex(k, l);
            prop(i, j, k, l) =
                _staged_output[batch * 36 + 6 * a + b] / (mandelFactor(a) * mandelFactor(b));
          }
}
