# MooseRevision.cmake
#
# Defines a build-time custom command that updates
#   framework/include/base/MooseRevision.h
# via framework/scripts/get_repo_revision.py.
#
# The script sets four environment variables and runs Python 3:
#   REPO_LOCATION     source root passed to git commands
#   HEADER_FILE       absolute path to the output header
#   APPLICATION_NAME  "MOOSE" → defines MOOSE_REVISION, MOOSE_VERSION, ...
#   INSTALLABLE_DIRS  "tests" → written into MOOSE_INSTALLABLE_DIRS
#
# The script has built-in no-op logic: when the revision string has not
# changed it touches .git/HEAD and .git/index back to the header's timestamp
# instead of rewriting the header. This prevents redundant downstream builds.
#
# Usage in framework/CMakeLists.txt:
#   include(MooseRevision)
#   add_dependencies(moose_framework moose_revision)
#
# The checked-in MooseRevision.h is valid until the first build. The custom
# command updates it whenever .git/HEAD or .git/index is newer than the header.

# -- Python 3 interpreter -------------------------------------------------
# Required to run get_repo_revision.py. Scoped to this include; no other
# file needs Python 3.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# -- Paths ----------------------------------------------------------------

set(MOOSE_REVISION_HEADER
    "${CMAKE_SOURCE_DIR}/framework/include/base/MooseRevision.h"
    CACHE INTERNAL "Path to the generated MooseRevision.h")

set(_moose_revision_script
    "${CMAKE_SOURCE_DIR}/framework/scripts/get_repo_revision.py")

# -- Git state triggers ---------------------------------------------------
# Depend on .git/HEAD (changes on checkout/branch switch) and .git/index
# (changes on git add / commit). If neither exists (release tarball, CI
# shallow clone without these files), the command has no file triggers and
# the existing checked-in header is used permanently without error.

set(_git_deps "")
foreach(_git_file
        "${CMAKE_SOURCE_DIR}/.git/HEAD"
        "${CMAKE_SOURCE_DIR}/.git/index")
    if(EXISTS "${_git_file}")
        list(APPEND _git_deps "${_git_file}")
    endif()
endforeach()

# -- Custom command -------------------------------------------------------
# Output is in the source tree to match the Make system. Framework source
# includes MooseRevision.h via the standard framework include path; moving
# the output to the build tree would require adding a build-tree include
# directory to moose_framework (deferred to a later phase if needed).

add_custom_command(
    OUTPUT  "${MOOSE_REVISION_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E env
                "REPO_LOCATION=${CMAKE_SOURCE_DIR}/framework"
                "HEADER_FILE=${MOOSE_REVISION_HEADER}"
                "APPLICATION_NAME=MOOSE"
                "INSTALLABLE_DIRS=tests"
            "${Python3_EXECUTABLE}" "${_moose_revision_script}"
    DEPENDS ${_git_deps}
    COMMENT "Updating MooseRevision.h"
    VERBATIM
)

# -- Custom target --------------------------------------------------------
# moose_revision is the dependency handle. framework/CMakeLists.txt calls:
#   add_dependencies(moose_framework moose_revision)

add_custom_target(moose_revision
    DEPENDS "${MOOSE_REVISION_HEADER}"
)
