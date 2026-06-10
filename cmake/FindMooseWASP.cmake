# FindMooseWASP.cmake
#
# Finds a pre-installed WASP library and produces IMPORTED target WASP::WASP.
# WASP is a required dependency and must be built before running cmake.
#
# Input cache variable:
#   WASP_DIR   Root of the WASP installation.
#              Resolution order: CMake cache → env WASP_DIR → <repo>/framework/contrib/wasp/install
#
# Produced:
#   WASP::WASP        IMPORTED INTERFACE target
#   MooseWASP_FOUND   TRUE on success

# -- Resolve WASP_DIR -----------------------------------------------------

if(NOT DEFINED WASP_DIR OR WASP_DIR STREQUAL "")
    if(DEFINED ENV{WASP_DIR} AND NOT "$ENV{WASP_DIR}" STREQUAL "")
        set(WASP_DIR "$ENV{WASP_DIR}"
            CACHE PATH "Root of the WASP installation")
    else()
        set(WASP_DIR "${CMAKE_SOURCE_DIR}/framework/contrib/wasp/install"
            CACHE PATH "Root of the WASP installation")
    endif()
endif()

# -- Verify headers -------------------------------------------------------

if(NOT IS_DIRECTORY "${WASP_DIR}/include")
    message(FATAL_ERROR
        "WASP is a required dependency but was not found.\n"
        "\n"
        "Expected installation at:\n"
        "  ${WASP_DIR}\n"
        "\n"
        "Read the dependency build instructions in MOOSE_MAKE_BUILD_GUIDE.md,\n"
        "then build WASP by running:\n"
        "  scripts/update_and_rebuild_wasp.sh\n"
        "\n"
        "If WASP is installed at a non-default location, pass its root:\n"
        "  cmake -DWASP_DIR=/path/to/wasp/install ...")
endif()

# -- Find WASP libraries --------------------------------------------------
# Prefer shared libraries. Fall back to static only when no shared libs exist.
# Keeping the two searches separate avoids linking both forms simultaneously.

file(GLOB _wasp_shared
    "${WASP_DIR}/lib/libwasp*${CMAKE_SHARED_LIBRARY_SUFFIX}")
file(GLOB _wasp_static
    "${WASP_DIR}/lib/libwasp*${CMAKE_STATIC_LIBRARY_SUFFIX}")

if(_wasp_shared)
    set(_wasp_libs "${_wasp_shared}")
elseif(_wasp_static)
    set(_wasp_libs "${_wasp_static}")
else()
    message(FATAL_ERROR
        "WASP headers found at:\n"
        "  ${WASP_DIR}/include\n"
        "but no libraries were found under:\n"
        "  ${WASP_DIR}/lib\n"
        "\n"
        "The WASP installation is incomplete. Re-run:\n"
        "  scripts/update_and_rebuild_wasp.sh")
endif()

# -- Produce IMPORTED target ----------------------------------------------

if(NOT TARGET WASP::WASP)
    add_library(WASP::WASP INTERFACE IMPORTED)
    set_target_properties(WASP::WASP PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${WASP_DIR}/include"
        INTERFACE_COMPILE_DEFINITIONS "WASP_ENABLED"
        # INTERFACE_LINK_DIRECTORIES adds -L${WASP_DIR}/lib so the linker can
        # resolve cross-references among WASP's own libraries at link time.
        INTERFACE_LINK_DIRECTORIES    "${WASP_DIR}/lib"
        # Runtime path so the loader finds WASP shared libraries after install.
        INTERFACE_LINK_OPTIONS        "-Wl,-rpath,${WASP_DIR}/lib"
        INTERFACE_LINK_LIBRARIES      "${_wasp_libs}"
    )
endif()

set(MooseWASP_FOUND TRUE)
message(STATUS "Found MooseWASP: ${WASP_DIR} (${_wasp_libs})")
