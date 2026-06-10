# MooseApplication.cmake
#
# Provides moose_add_application() — the CMake equivalent of framework/app.mk.
# Call this once per application or module to produce:
#
#   ${NAME}            SHARED library from SRC_DIR/**/*.C
#   ${NAME}_exe        EXECUTABLE (optional) — the simulation binary
#   ${NAME}_test_lib   SHARED library from TEST_SRC_DIR/**/*.C (optional)
#   ${NAME}_test_exe   EXECUTABLE linking the main lib + test lib (optional)
#   ${NAME}_unit       EXECUTABLE GTest runner from UNIT_SRC_DIR/**/*.C (optional)
#
# Usage:
#   moose_add_application(
#     NAME          <name>
#     SRC_DIR       <path>
#     INCLUDE_DIR   <path>
#     LINK_LIBS     <target> [<target> ...]
#     [EXECUTABLE]
#     [EXECUTABLE_MAIN  <path/to/main.C>]
#     [TEST_SRC_DIR     <path>]
#     [TEST_INCLUDE_DIR <path>]
#     [UNIT_SRC_DIR     <path>]
#     [DEFINES          <DEF> ...]
#     [EXCLUDE_SRC_DIRS <reldir> ...]   # relative to SRC_DIR; e.g. kokkos libtorch
#   )

function(moose_add_application)
    cmake_parse_arguments(APP
        "EXECUTABLE"
        "NAME;SRC_DIR;INCLUDE_DIR;EXECUTABLE_MAIN;TEST_SRC_DIR;UNIT_SRC_DIR"
        "LINK_LIBS;DEFINES;EXCLUDE_SRC_DIRS;TEST_INCLUDE_DIR"
        ${ARGN}
    )

    # -------------------------------------------------------------------------
    # 1. Collect main sources
    # -------------------------------------------------------------------------
    file(GLOB_RECURSE _srcs CONFIGURE_DEPENDS
        "${APP_SRC_DIR}/*.C"
    )

    # Remove excluded subdirectories (e.g. kokkos, libtorch, neml2)
    foreach(_excl IN LISTS APP_EXCLUDE_SRC_DIRS)
        list(FILTER _srcs EXCLUDE REGEX ".*/src/${_excl}/.*")
    endforeach()

    # Remove main.C — it goes into the executable, not the library
    if(APP_EXECUTABLE_MAIN)
        list(REMOVE_ITEM _srcs "${APP_EXECUTABLE_MAIN}")
    endif()

    # -------------------------------------------------------------------------
    # 1b. Merge test sources into main library (module pattern)
    #
    # In the MOOSE module pattern, src/main.C instantiates the *Test* app
    # (e.g. MiscTestApp), so test sources and their include dirs must be
    # part of the main library and its executable — not a separate target.
    # This is the case when TEST_SRC_DIR exists but has no main.C of its own.
    # -------------------------------------------------------------------------
    set(_test_inc_dirs "")
    set(_skip_section5 FALSE)
    if(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}")
        set(_test_main_candidate "${APP_TEST_SRC_DIR}/main.C")
        if(NOT EXISTS "${_test_main_candidate}")
            # Module pattern: merge test sources into _srcs.
            file(GLOB_RECURSE _test_merge_srcs CONFIGURE_DEPENDS
                "${APP_TEST_SRC_DIR}/*.C"
            )
            foreach(_excl IN LISTS APP_EXCLUDE_SRC_DIRS)
                list(FILTER _test_merge_srcs EXCLUDE REGEX ".*/src/${_excl}/.*")
            endforeach()
            list(APPEND _srcs ${_test_merge_srcs})

            # Collect test include dirs (needed by merged sources and by main.C).
            foreach(_test_inc_path IN LISTS APP_TEST_INCLUDE_DIR)
                if(EXISTS "${_test_inc_path}")
                    file(GLOB_RECURSE _t_inc_candidates
                        LIST_DIRECTORIES true
                        CONFIGURE_DEPENDS
                        "${_test_inc_path}/*"
                    )
                    list(APPEND _test_inc_dirs "${_test_inc_path}")
                    foreach(_d IN LISTS _t_inc_candidates)
                        if(IS_DIRECTORY "${_d}")
                            list(APPEND _test_inc_dirs "${_d}")
                        endif()
                    endforeach()
                endif()
            endforeach()

            set(_skip_section5 TRUE)
        endif()
    endif()

    # -------------------------------------------------------------------------
    # 2. Collect include directories (all subdirs of INCLUDE_DIR)
    # -------------------------------------------------------------------------
    file(GLOB_RECURSE _inc_candidates
        LIST_DIRECTORIES true
        CONFIGURE_DEPENDS
        "${APP_INCLUDE_DIR}/*"
    )
    set(_inc_dirs "${APP_INCLUDE_DIR}")
    foreach(_d IN LISTS _inc_candidates)
        if(IS_DIRECTORY "${_d}")
            list(APPEND _inc_dirs "${_d}")
        endif()
    endforeach()

    # -------------------------------------------------------------------------
    # 3. Shared library target
    # -------------------------------------------------------------------------
    add_library(${APP_NAME} SHARED ${_srcs})

    target_include_directories(${APP_NAME}
        PUBLIC  ${_inc_dirs}
        PRIVATE ${_test_inc_dirs}
    )

    target_link_libraries(${APP_NAME}
        PUBLIC ${APP_LINK_LIBS}
    )

    if(APP_DEFINES)
        target_compile_definitions(${APP_NAME} PRIVATE ${APP_DEFINES})
    endif()

    set_target_properties(${APP_NAME} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    # -------------------------------------------------------------------------
    # 4. Executable (optional)
    # -------------------------------------------------------------------------
    if(APP_EXECUTABLE AND APP_EXECUTABLE_MAIN)
        add_executable(${APP_NAME}_exe "${APP_EXECUTABLE_MAIN}")
        set_target_properties(${APP_NAME}_exe PROPERTIES OUTPUT_NAME "${APP_NAME}")

        target_link_libraries(${APP_NAME}_exe PRIVATE ${APP_NAME})

        # Propagate include dirs so main.C can include the app header
        target_include_directories(${APP_NAME}_exe PRIVATE ${_inc_dirs} ${_test_inc_dirs})
    endif()

    # -------------------------------------------------------------------------
    # 5. Test library + executable (optional, framework pattern only)
    # -------------------------------------------------------------------------
    if(APP_TEST_SRC_DIR AND EXISTS "${APP_TEST_SRC_DIR}" AND NOT _skip_section5)
        file(GLOB_RECURSE _test_srcs CONFIGURE_DEPENDS
            "${APP_TEST_SRC_DIR}/*.C"
        )

        # Collect test include dirs
        set(_test_inc_dirs "")
        if(APP_TEST_INCLUDE_DIR AND EXISTS "${APP_TEST_INCLUDE_DIR}")
            file(GLOB_RECURSE _test_inc_candidates
                LIST_DIRECTORIES true
                CONFIGURE_DEPENDS
                "${APP_TEST_INCLUDE_DIR}/*"
            )
            set(_test_inc_dirs "${APP_TEST_INCLUDE_DIR}")
            foreach(_d IN LISTS _test_inc_candidates)
                if(IS_DIRECTORY "${_d}")
                    list(APPEND _test_inc_dirs "${_d}")
                endif()
            endforeach()
        endif()

        # Remove test main.C from the test library sources
        set(_test_main "${APP_TEST_SRC_DIR}/main.C")
        list(REMOVE_ITEM _test_srcs "${_test_main}")

        if(_test_srcs)
            add_library(${APP_NAME}_test_lib SHARED ${_test_srcs})
            target_include_directories(${APP_NAME}_test_lib
                PUBLIC ${_test_inc_dirs} ${_inc_dirs}
            )
            target_link_libraries(${APP_NAME}_test_lib PUBLIC ${APP_NAME})
            set_target_properties(${APP_NAME}_test_lib PROPERTIES
                POSITION_INDEPENDENT_CODE ON
            )
        else()
            # No test objects besides main — create an INTERFACE alias so the
            # test executable can still link cleanly
            add_library(${APP_NAME}_test_lib INTERFACE)
            target_link_libraries(${APP_NAME}_test_lib INTERFACE ${APP_NAME})
        endif()

        # Test executable: links the main library + test library
        if(EXISTS "${_test_main}")
            add_executable(${APP_NAME}_test_exe "${_test_main}")
            set_target_properties(${APP_NAME}_test_exe PROPERTIES
                OUTPUT_NAME "${APP_NAME}_test"
            )
            target_include_directories(${APP_NAME}_test_exe PRIVATE
                ${_test_inc_dirs} ${_inc_dirs}
            )
            target_link_libraries(${APP_NAME}_test_exe PRIVATE
                ${APP_NAME}_test_lib
            )
        endif()
    endif()

    # -------------------------------------------------------------------------
    # 6. Unit GTest executable (optional)
    # -------------------------------------------------------------------------
    if(APP_UNIT_SRC_DIR AND EXISTS "${APP_UNIT_SRC_DIR}")
        file(GLOB_RECURSE _unit_srcs CONFIGURE_DEPENDS
            "${APP_UNIT_SRC_DIR}/*.C"
        )

        if(_unit_srcs)
            add_executable(${APP_NAME}_unit ${_unit_srcs})

            # Unit tests need the main app's include dirs
            target_include_directories(${APP_NAME}_unit PRIVATE ${_inc_dirs})

            target_link_libraries(${APP_NAME}_unit PRIVATE
                ${APP_NAME}
                moose_gtest
            )

            add_test(NAME ${APP_NAME}_unit COMMAND ${APP_NAME}_unit)
        endif()
    endif()

endfunction()
