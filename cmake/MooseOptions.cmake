# Declare MOOSE build options.
# These correspond to the ENABLE_* variables in the Make system.
# All optional features default OFF; their source directories are excluded
# from the build when disabled, matching Make-era behavior.

option(MOOSE_ENABLE_KOKKOS
    "Build Kokkos-accelerated sources (requires KOKKOS_DIR)" OFF)
option(MOOSE_ENABLE_LIBTORCH
    "Build LibTorch integration sources (requires LIBTORCH_DIR)" OFF)
option(MOOSE_ENABLE_MFEM
    "Build MFEM integration sources (requires MFEM_DIR)" OFF)
option(MOOSE_ENABLE_NEML2
    "Build NEML2 integration sources (requires NEML2_DIR)" OFF)

# Map CMAKE_BUILD_TYPE to the MOOSE METHOD string used across the codebase.
# Allowed values match the Make build: opt dbg devel oprof.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_moose_method_default "dbg")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(_moose_method_default "oprof")
else()
    set(_moose_method_default "opt")  # Release, unset (Ninja default), or anything else
endif()
set(MOOSE_METHOD "${_moose_method_default}" CACHE STRING
    "MOOSE build method string (opt|dbg|devel|oprof). Passed as -DMETHOD= to all targets.")
set_property(CACHE MOOSE_METHOD PROPERTY STRINGS opt dbg devel oprof)
