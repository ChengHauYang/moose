# MooseModule.cmake
#
# Provides moose_add_module() — a thin wrapper around moose_add_application()
# with module-specific path conventions and feature-flag exclusions.
#
# Usage:
#   moose_add_module(
#     NAME     <module-name>           # e.g. solid_mechanics
#     [DEPENDS <target> ...]           # other module CMake targets
#   )
#
# Conventions assumed for a module named NAME:
#   Source:        modules/NAME/src/
#   Headers:       modules/NAME/include/
#   Entry point:   modules/NAME/src/main.C
#   Test sources:  modules/NAME/test/src/   (if directory exists)
#   Test headers:  modules/NAME/test/include/  (if directory exists)
#   Unit tests:    modules/NAME/unit/src/   (if directory exists)

include(MooseApplication)

function(moose_add_module)
    cmake_parse_arguments(MOD "" "NAME" "DEPENDS;ADDITIONAL_TEST_INCLUDES" ${ARGN})

    set(_src_dir  "${CMAKE_SOURCE_DIR}/modules/${MOD_NAME}/src")
    set(_inc_dir  "${CMAKE_SOURCE_DIR}/modules/${MOD_NAME}/include")
    set(_test_src "${CMAKE_SOURCE_DIR}/modules/${MOD_NAME}/test/src")
    set(_test_inc "${CMAKE_SOURCE_DIR}/modules/${MOD_NAME}/test/include")
    set(_unit_src "${CMAKE_SOURCE_DIR}/modules/${MOD_NAME}/unit/src")
    set(_main_c   "${_src_dir}/main.C")

    # Link against the framework plus any declared module dependencies
    set(_link_libs moose_framework ${MOD_DEPENDS})

    # Optional subdirectories — only passed to moose_add_application if present
    set(_opt_args "")

    if(EXISTS "${_test_src}")
        list(APPEND _opt_args TEST_SRC_DIR "${_test_src}")
    endif()

    set(_test_incs "")
    if(EXISTS "${_test_inc}")
        list(APPEND _test_incs "${_test_inc}")
    endif()
    if(MOD_ADDITIONAL_TEST_INCLUDES)
        list(APPEND _test_incs ${MOD_ADDITIONAL_TEST_INCLUDES})
    endif()

    if(_test_incs)
        list(APPEND _opt_args TEST_INCLUDE_DIR ${_test_incs})
    endif()

    if(EXISTS "${_unit_src}")
        list(APPEND _opt_args UNIT_SRC_DIR "${_unit_src}")
    endif()

    # Exclude optional-feature source directories that are not enabled.
    # Mirrors the per-directory exclusion Phase 1 does for moose_framework.
    set(_exclude "")
    if(NOT MOOSE_ENABLE_KOKKOS)
        list(APPEND _exclude "kokkos")
    endif()
    if(NOT MOOSE_ENABLE_LIBTORCH)
        list(APPEND _exclude "libtorch")
    endif()
    if(NOT MOOSE_ENABLE_NEML2)
        list(APPEND _exclude "neml2")
    endif()
    if(NOT MOOSE_ENABLE_MFEM)
        list(APPEND _exclude "mfem")
    endif()

    moose_add_application(
        NAME              ${MOD_NAME}
        SRC_DIR           "${_src_dir}"
        INCLUDE_DIR       "${_inc_dir}"
        LINK_LIBS         ${_link_libs}
        EXECUTABLE
        EXECUTABLE_MAIN   "${_main_c}"
        EXCLUDE_SRC_DIRS  ${_exclude}
        ${_opt_args}
    )
endfunction()
