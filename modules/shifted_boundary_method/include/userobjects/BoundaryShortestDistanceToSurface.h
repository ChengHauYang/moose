//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "SideUserObject.h"

#ifdef MOOSE_KOKKOS_ENABLED
#include "KokkosSurrogateInterfaceMapping.h"
#endif

#include <memory>
#include <vector>

class Function;
class SBMInterfaceManager;

#ifdef MOOSE_KOKKOS_ENABLED
namespace Moose::Kokkos
{
struct InterfaceFaceRecord;
}
#endif

class BoundaryShortestDistanceToSurface : public SideUserObject
{
public:
  using ElemSide = std::pair<dof_id_type, unsigned short int>;

  static InputParameters validParams();
  BoundaryShortestDistanceToSurface(const InputParameters & parameters);

  virtual void initialSetup() override;
  virtual void execute() override;
  virtual void finalize() override;
  virtual void initialize() override;
  virtual void threadJoin(const UserObject & uo) override;

  /// Provides query interfaces for distance vector
  const RealVectorValue & surrogateDistance(const ElemSide & elem_side, unsigned int qp) const;
  /// Provides query interfaces for normal vector
  const RealVectorValue & trueNormal(const ElemSide & elem_side, unsigned int qp) const;

#ifdef MOOSE_KOKKOS_ENABLED
  /**
   * Register and allocate a surrogate-interface mapping for a Kokkos interface boundary
   * restriction. The mapping is populated after this user object finalizes its globally oriented
   * normals.
   */
  const Moose::Kokkos::SurrogateInterfaceMapping & registerKokkosSurrogateInterfaceMapping(
      const std::set<BoundaryID> & boundary_ids,
      const Moose::Kokkos::Array<Moose::Kokkos::InterfaceFaceRecord> & faces) const;

  /// Whether registered surrogate-interface mappings have been populated for a boundary
  /// restriction.
  bool kokkosSurrogateInterfaceMappingReady(const std::set<BoundaryID> & boundary_ids) const;
#endif

protected:
  /// Local distance functions when no external distance user object is supplied.
  std::vector<const Function *> _distance_functions;

  /// distance
  std::map<ElemSide, std::vector<RealVectorValue>> _distance_vectors;

  /// normal
  mutable std::map<ElemSide, std::vector<RealVectorValue>> _normal_vectors;

  /// side id to index map
  std::map<BoundaryID, unsigned int> _side_id_index;

  /// Optional manager supplying geometry for interfaces in a single surface mesh.
  const SBMInterfaceManager * _manager = nullptr;

  /// Ordered subdomain pair associated with each surrogate boundary.
  std::map<BoundaryID, std::pair<SubdomainID, SubdomainID>> _boundary_subdomain_pairs;

private:
  /// Signed true-interface measure integrated over each surrogate boundary.
  std::vector<Real> _true_interface_measures;

  /// Map from ElemSide to boundary ID index.
  std::map<ElemSide, unsigned int> _elem_side_to_bid;

#ifdef MOOSE_KOKKOS_ENABLED
  /// Populate registered surrogate-interface mappings after normal orientation has been finalized.
  void populateSurrogateInterfaceMappings();

  /// Surrogate-interface mapping caches are separated by boundary set; this UO's identity
  /// supplies the other key.
  mutable std::map<std::set<BoundaryID>, std::shared_ptr<Moose::Kokkos::SurrogateInterfaceMapping>>
      _kokkos_surrogate_interface_mappings;
#endif

  /// @brief Whether to suppress warnings about large distances.
  bool _suppress_distance_warning;

  /// @brief Whether to output debug information.
  bool _debug_output;
};
