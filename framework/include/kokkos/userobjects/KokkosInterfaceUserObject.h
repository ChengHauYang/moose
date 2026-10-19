//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "KokkosInterfaceDatum.h"
#include "KokkosInterfaceFaceCache.h"
#include "KokkosUserObject.h"

#include "BoundaryRestrictableRequired.h"
#include "CoupleableMooseVariableDependencyIntermediateInterface.h"
#include "TwoMaterialPropertyInterface.h"

namespace Moose::Kokkos
{

/** Base class for Kokkos user objects that operate on both sides of an interface face. */
class InterfaceUserObject : public UserObject,
                            public MeshHolder,
                            public AssemblyHolder,
                            public FESystemHolder,
                            public ::BoundaryRestrictableRequired,
                            public ::CoupleableMooseVariableDependencyIntermediateInterface,
                            public ::TwoMaterialPropertyInterface
{
public:
  static InputParameters validParams();

  InterfaceUserObject(const InputParameters & parameters);
  InterfaceUserObject(const InterfaceUserObject & object);

  void compute() override;

  /** Kokkos function tag for two-sided interface user-object execution. */
  struct InterfaceLoop
  {
  };

  /**
   * Default method to prevent compile errors when a derived object does not define its interface
   * execution hook.
   */
  template <typename Derived>
  KOKKOS_FUNCTION void executeInterface(Datum & /* primary */,
                                        Datum & /* neighbor */,
                                        const InterfaceFaceRecord & /* face */) const
  {
    ::Kokkos::abort("Default executeInterface() should never be called. Make sure you properly "
                    "redefined this method in your class without typos.");
  }

  template <typename Derived>
  static auto defaultExecuteInterface()
  {
    return &InterfaceUserObject::executeInterface<Derived>;
  }

  /** The parallel computation entry function called by Kokkos. */
  template <typename Derived>
  KOKKOS_FUNCTION void operator()(InterfaceLoop, const ThreadID tid, const Derived & object) const;

protected:
  virtual void computeUserObject() override;
  virtual ThreadID numUserObjectThreads() const override { return numKokkosInterfaceFaces(); }

  KOKKOS_FUNCTION ThreadID numKokkosInterfaceFaces() const { return _interface_faces.size(); }

  KOKKOS_FUNCTION const InterfaceFaceRecord & kokkosInterfaceFace(const ThreadID tid) const
  {
    return _interface_faces[tid];
  }

  KOKKOS_FUNCTION unsigned int kokkosInterfaceNeighborQp(const InterfaceFaceRecord & face,
                                                         const unsigned int qp) const
  {
    return _neighbor_qp_map[face.qp_map_offset + qp];
  }

private:
  void acquireInterfaceFaces();

  /// Shallow handles into the face cache shared with Kokkos interface materials.
  Array<InterfaceFaceRecord> _interface_faces;
  /// Neighbor-side quadrature-point index for each primary-side quadrature point.
  Array<unsigned int> _neighbor_qp_map;
  std::unique_ptr<DispatcherBase> _interface_user_object_dispatcher;
};

template <typename Derived>
KOKKOS_FUNCTION void
InterfaceUserObject::operator()(InterfaceLoop, const ThreadID tid, const Derived & object) const
{
  const auto & face = kokkosInterfaceFace(tid);
  auto primary = makePrimaryInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());
  auto neighbor = makeNeighborInterfaceDatum<Datum>(face, kokkosAssembly(), kokkosSystems());

  object.template executeInterface<Derived>(primary, neighbor, face);
}

} // namespace Moose::Kokkos

#define callRegisterKokkosInterfaceUserObjectFunction(classname, objectname)                       \
  static char registerKokkosInterfaceUserObject##classname()                                       \
  {                                                                                                \
    using namespace Moose::Kokkos;                                                                 \
                                                                                                   \
    DispatcherRegistry::addDispatcher<classname::InterfaceLoop, classname>(objectname);            \
    DispatcherRegistry::hasUserMethod<classname::InterfaceLoop>(                                   \
        objectname,                                                                                \
        &classname::executeInterface<classname> !=                                                 \
            classname::defaultExecuteInterface<classname>());                                      \
                                                                                                   \
    return 0;                                                                                      \
  }                                                                                                \
                                                                                                   \
  [[maybe_unused]] static char combineNames(kokkos_dispatcher_interface_userobject_##classname,    \
                                            __COUNTER__) =                                         \
      registerKokkosInterfaceUserObject##classname()

#define registerKokkosInterfaceUserObject(app, classname)                                          \
  registerMooseObject(app, classname);                                                             \
  callRegisterKokkosInterfaceUserObjectFunction(classname, #classname)

#define registerKokkosInterfaceUserObjectAliased(app, classname, alias)                            \
  registerMooseObjectAliased(app, classname, alias);                                               \
  callRegisterKokkosInterfaceUserObjectFunction(classname, alias)
