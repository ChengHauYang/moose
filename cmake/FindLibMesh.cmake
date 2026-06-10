# FindLibMesh.cmake
#
# Wraps libmesh-config to produce an IMPORTED INTERFACE target LibMesh::LibMesh.
# This is the single point in the CMake graph that shells out to libmesh-config.
#
# Input cache variable:
#   LIBMESH_DIR   Root of the libmesh installation.
#                 Resolution order: CMake cache → env LIBMESH_DIR → <repo>/libmesh/installed
#
# Expected tool:
#   ${LIBMESH_DIR}/bin/libmesh-config
#
# Produced:
#   LibMesh::LibMesh          IMPORTED INTERFACE target
#   MOOSE_CXX_STANDARD        integer (≥17), cached INTERNAL, derived from --cxxflags
#   LibMesh_FOUND             TRUE on success
#
# Parsing policy (conservative):
#   Compile pass (--include, --cppflags, --cxxflags):
#     -I*   → INTERFACE_INCLUDE_DIRECTORIES
#     -D*   → INTERFACE_COMPILE_DEFINITIONS
#     -std=c++XX  → extracted, set MOOSE_CXX_STANDARD, not forwarded (CMake owns this)
#     rest  → INTERFACE_COMPILE_OPTIONS  (passed through verbatim)
#   Link pass (--ldflags):
#     everything → INTERFACE_LINK_OPTIONS  (no reinterpretation)
#   Link pass (--libs):
#     -l*   → INTERFACE_LINK_LIBRARIES
#     rest  → INTERFACE_LINK_OPTIONS

# -- Resolve LIBMESH_DIR --------------------------------------------------
# Resolution order:
#   1. -DLIBMESH_DIR=... passed explicitly on the cmake command line (cache)
#   2. $LIBMESH_DIR environment variable (with warn-and-fallback, see below)
#   3. <repo>/libmesh/installed  — default output of scripts/update_and_rebuild_libmesh.sh
#                                  (NOT produced by git submodule update alone)

set(_lm_repo_default "${CMAKE_SOURCE_DIR}/libmesh/installed")

if(NOT DEFINED LIBMESH_DIR OR LIBMESH_DIR STREQUAL "")
    if(DEFINED ENV{LIBMESH_DIR} AND NOT "$ENV{LIBMESH_DIR}" STREQUAL "")
        set(LIBMESH_DIR "$ENV{LIBMESH_DIR}"
            CACHE PATH "Root of the libmesh installation")
    else()
        set(LIBMESH_DIR "${_lm_repo_default}"
            CACHE PATH "Root of the libmesh installation")
    endif()
endif()

# -- Locate libmesh-config ------------------------------------------------
# Construct the expected path directly rather than using find_program, to
# avoid stale cache entries when LIBMESH_DIR is changed between runs.
# Search order within a given LIBMESH_DIR:
#   bin/libmesh-config        — installed layout (scripts/update_and_rebuild_libmesh.sh)
#   contrib/bin/libmesh-config — uninstalled/build-tree layout

set(_lm_config "${LIBMESH_DIR}/bin/libmesh-config")
if(NOT EXISTS "${_lm_config}")
    set(_lm_config "${LIBMESH_DIR}/contrib/bin/libmesh-config")
endif()

if(NOT EXISTS "${_lm_config}")
    # libmesh-config was not found under LIBMESH_DIR.
    #
    # If LIBMESH_DIR came from the environment variable (not an explicit
    # -DLIBMESH_DIR= on the command line), it may be a stale shell export
    # pointing at an unrelated build tree.  In that case we silently fall back
    # to the bundled submodule default and warn the user so the behaviour is
    # visible but not fatal.
    #
    # If LIBMESH_DIR was set explicitly on the command line we treat that as
    # intentional and fail immediately rather than silently ignoring it.
    if(DEFINED ENV{LIBMESH_DIR} AND LIBMESH_DIR STREQUAL "$ENV{LIBMESH_DIR}"
       AND NOT LIBMESH_DIR STREQUAL _lm_repo_default)
        message(WARNING
            "LIBMESH_DIR is set (via environment variable) to:\n"
            "  ${LIBMESH_DIR}\n"
            "but libmesh-config was not found at either of:\n"
            "  ${LIBMESH_DIR}/bin/libmesh-config\n"
            "  ${LIBMESH_DIR}/contrib/bin/libmesh-config\n"
            "\n"
            "Falling back to the bundled submodule default:\n"
            "  ${_lm_repo_default}\n"
            "\n"
            "To suppress this warning and use an external libmesh, pass its\n"
            "root explicitly on the cmake command line (this overrides the env var):\n"
            "  cmake -DLIBMESH_DIR=/path/to/libmesh/installed ...")
        set(LIBMESH_DIR "${_lm_repo_default}"
            CACHE PATH "Root of the libmesh installation" FORCE)
        set(_lm_config "${LIBMESH_DIR}/bin/libmesh-config")
        if(NOT EXISTS "${_lm_config}")
            set(_lm_config "${LIBMESH_DIR}/contrib/bin/libmesh-config")
        endif()
    endif()
endif()

if(NOT EXISTS "${_lm_config}")
    message(FATAL_ERROR
        "libmesh-config not found. Searched:\n"
        "  ${LIBMESH_DIR}/bin/libmesh-config\n"
        "  ${LIBMESH_DIR}/contrib/bin/libmesh-config\n"
        "\n"
        "Build libmesh from the bundled submodule by running:\n"
        "  scripts/update_and_rebuild_libmesh.sh\n"
        "\n"
        "That script installs to libmesh/installed/, which is the default\n"
        "LIBMESH_DIR. To use an external libmesh instead, pass its root\n"
        "explicitly on the cmake command line:\n"
        "  cmake -DLIBMESH_DIR=/path/to/libmesh/installed ...")
endif()

# -- Execute libmesh-config -----------------------------------------------

foreach(_query include cppflags cxxflags ldflags libs)
    execute_process(
        COMMAND "${_lm_config}" "--${_query}"
        OUTPUT_VARIABLE  _lm_${_query}_raw
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE  _rc
    )
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR
            "libmesh-config --${_query} exited with status ${_rc}\n"
            "Verify your libmesh installation at ${LIBMESH_DIR}.")
    endif()
    separate_arguments(_lm_${_query} UNIX_COMMAND "${_lm_${_query}_raw}")
endforeach()

# -- Compile pass: --include, --cppflags, --cxxflags ----------------------
# Flags in this pass are consumed by the compiler, not the linker.
# -W* here means compiler warning flags; they go to compile options.

set(_lm_include_dirs "")
set(_lm_compile_defs "")
set(_lm_compile_opts "")
set(_lm_cxx_std      "")

foreach(_flag ${_lm_include} ${_lm_cppflags} ${_lm_cxxflags})
    if(_flag MATCHES "^-I(.+)$")
        list(APPEND _lm_include_dirs "${CMAKE_MATCH_1}")
    elseif(_flag MATCHES "^-D(.+)$")
        list(APPEND _lm_compile_defs "${CMAKE_MATCH_1}")
    elseif(_flag MATCHES "^-std=c\\+\\+([0-9]+)$")
        set(_lm_cxx_std "${CMAKE_MATCH_1}")
        # Intentionally not forwarded: CMAKE_CXX_STANDARD owns this.
    else()
        list(APPEND _lm_compile_opts "${_flag}")
    endif()
endforeach()

list(REMOVE_DUPLICATES _lm_include_dirs)
list(REMOVE_DUPLICATES _lm_compile_defs)

# -- Link pass: --ldflags -------------------------------------------------
# Conservative passthrough. Do not reinterpret -Wl,…, -L, -rpath, etc.
# The linker understands all of these directly.

set(_lm_link_opts "")

foreach(_flag ${_lm_ldflags})
    list(APPEND _lm_link_opts "${_flag}")
endforeach()

# -- Link pass: --libs ----------------------------------------------------
# -l* are clean library references; route to INTERFACE_LINK_LIBRARIES so
# CMake can deduplicate them. Anything else (rare) passes through to opts.

set(_lm_link_libs "")

foreach(_flag ${_lm_libs})
    if(_flag MATCHES "^-l(.+)$")
        list(APPEND _lm_link_libs "${_flag}")
    else()
        list(APPEND _lm_link_opts "${_flag}")
    endif()
endforeach()

# -- Enforce minimum C++17 ------------------------------------------------

if(_lm_cxx_std STREQUAL "" OR _lm_cxx_std LESS 17)
    if(NOT _lm_cxx_std STREQUAL "")
        message(STATUS
            "libmesh reports C++${_lm_cxx_std}; "
            "MOOSE requires C++17 minimum — overriding.")
    endif()
    set(_lm_cxx_std 17)
endif()

# FORCE so that swapping the libmesh installation updates the cached value.
set(MOOSE_CXX_STANDARD "${_lm_cxx_std}"
    CACHE INTERNAL "C++ standard integer derived from libmesh-config" FORCE)

# -- Produce IMPORTED target ----------------------------------------------

if(NOT TARGET LibMesh::LibMesh)
    add_library(LibMesh::LibMesh INTERFACE IMPORTED)
    set_target_properties(LibMesh::LibMesh PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_lm_include_dirs}"
        INTERFACE_COMPILE_DEFINITIONS "${_lm_compile_defs}"
        INTERFACE_COMPILE_OPTIONS     "${_lm_compile_opts}"
        INTERFACE_LINK_OPTIONS        "${_lm_link_opts}"
        INTERFACE_LINK_LIBRARIES      "${_lm_link_libs}"
    )
endif()

set(LibMesh_FOUND TRUE)
message(STATUS "Found LibMesh: ${_lm_config} (C++${MOOSE_CXX_STANDARD})")
