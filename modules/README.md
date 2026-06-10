# MOOSE Modules

This directory contains the various physics modules for the MOOSE framework. Each module is a self-contained library that provides specific physics kernels, boundary conditions, and materials.

## Module Dependencies

MOOSE modules can depend on each other. These dependencies are authoritative defined in `modules/modules.mk`. When building a module, its dependencies must be compiled and linked.

### Dependency Table

The following table lists modules that have explicit dependencies on other modules:

| Module | Dependencies |
| :--- | :--- |
| **contact** | `solid_mechanics` |
| **fluid_properties** | `misc` |
| **fsi** (Fluid-Structure Interaction) | `navier_stokes`, `solid_mechanics` |
| **heat_transfer** | `ray_tracing` |
| **navier_stokes** | `fluid_properties`, `rdg`, `heat_transfer` |
| **peridynamics** | `solid_mechanics` |
| **phase_field** | `solid_mechanics` |
| **porous_flow** | `solid_mechanics`, `fluid_properties`, `chemical_reactions` |
| **scalar_transport** | `chemical_reactions`, `navier_stokes`, `thermal_hydraulics`, `fluid_properties`, `heat_transfer`, `rdg`, `ray_tracing`, `solid_properties`, `misc` |
| **solid_properties** | `heat_transfer` |
| **subchannel** | `fluid_properties`, `heat_transfer`, `reactor` |
| **thermal_hydraulics** | `navier_stokes`, `fluid_properties`, `heat_transfer`, `rdg`, `ray_tracing`, `solid_properties`, `misc` |
| **xfem** | `solid_mechanics` |
| **combined** | Special aggregator that includes **all** modules (`ALL_MODULES := yes`) |

### Independent Modules

The following modules do not depend on any other physics modules (they only depend on the MOOSE framework):

*   `chemical_reactions`
*   `electromagnetics`
*   `external_petsc_solver`
*   `functional_expansion_tools`
*   `geochemistry`
*   `level_set`
*   `misc`
*   `optimization`
*   `ray_tracing`
*   `rdg`
*   `reactor`
*   `solid_mechanics`
*   `stochastic_tools`

## Deprecated / Renamed Modules

For historical reasons, some modules have been renamed. The old names should no longer be used in new Makefiles:

*   **heat_conduction** -> Renamed to **heat_transfer**
*   **tensor_mechanics** -> Renamed to **solid_mechanics**

## Compilation Note

When building an individual module via Make, the dependencies are automatically handled by `modules/modules.mk`. Ensure that `MODULE_NAME` is correctly set in your module's `Makefile` before including `modules.mk`.

For more information on the build system, refer to `MOOSE_MAKE_BUILD_GUIDE.md` in the repository root.
