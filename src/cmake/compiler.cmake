# Copyright Contributors to the Open Shading Language project.
# SPDX-License-Identifier: BSD-3-Clause
# https://github.com/AcademySoftwareFoundation/OpenShadingLanguage

###########################################################################
#
# This file contains compiler-related detection, options, and actions.
#
# Each option declaration is kept close to the related logic for that
# option.
#
###########################################################################


###########################################################################
# Print some basic status about the system and compiler
#
if (VERBOSE)
    message (STATUS "CMAKE_SYSTEM_NAME      = ${CMAKE_SYSTEM_NAME}")
    message (STATUS "CMAKE_SYSTEM_VERSION   = ${CMAKE_SYSTEM_VERSION}")
    message (STATUS "CMAKE_SYSTEM_PROCESSOR = ${CMAKE_SYSTEM_PROCESSOR}")
endif ()
message (STATUS "CMAKE_CXX_COMPILER     = ${CMAKE_CXX_COMPILER}")
message (STATUS "CMAKE_CXX_COMPILER_ID  = ${CMAKE_CXX_COMPILER_ID}")


###########################################################################
# C++ language standard
#
set (CMAKE_CXX_STANDARD 14 CACHE STRING
     "C++ standard to build with (14, 17, 20, etc.)")
set (DOWNSTREAM_CXX_STANDARD 14 CACHE STRING
     "C++ minimum standard to impose on downstream clients")
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
message (STATUS "Building with C++${CMAKE_CXX_STANDARD}, downstream minimum C++${DOWNSTREAM_CXX_STANDARD}")


###########################################################################
# Figure out which compiler we're using
#

if (CMAKE_COMPILER_IS_GNUCC)
    execute_process (COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                     OUTPUT_VARIABLE GCC_VERSION
                     OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (VERBOSE)
        message (STATUS "Using gcc ${GCC_VERSION} as the compiler")
    endif ()
else ()
    set (GCC_VERSION 0)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER MATCHES "[Cc]lang"
    OR CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
    # If using any flavor of clang, set CMAKE_COMPILER_IS_CLANG. If it's
    # Apple's variety, set CMAKE_COMPILER_IS_APPLECLANG and
    # APPLECLANG_VERSION_STRING, otherwise for generic clang set
    # CLANG_VERSION_STRING.
    set (CMAKE_COMPILER_IS_CLANG 1)
    EXECUTE_PROCESS( COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE clang_full_version_string )
    if (clang_full_version_string MATCHES "Apple")
        set (CMAKE_CXX_COMPILER_ID "AppleClang")
        set (CMAKE_COMPILER_IS_APPLECLANG 1)
        string (REGEX REPLACE ".* version ([0-9]+\\.[0-9]+).*" "\\1" APPLECLANG_VERSION_STRING ${clang_full_version_string})
        if (VERBOSE)
            message (STATUS "The compiler is Clang: ${CMAKE_CXX_COMPILER_ID} version ${APPLECLANG_VERSION_STRING}")
        endif ()
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "IntelLLVM")
        set (CMAKE_COMPILER_IS_INTELCLANG 1)
        string (REGEX MATCH "[0-9]+(\\.[0-9]+)+" INTELCLANG_VERSION_STRING ${clang_full_version_string})
        if (VERBOSE)
            message (STATUS "The compiler is Intel Clang: ${CMAKE_CXX_COMPILER_ID} version ${INTELCLANG_VERSION_STRING}")
        endif ()
    else ()
        string (REGEX REPLACE ".* version ([0-9]+\\.[0-9]+).*" "\\1" CLANG_VERSION_STRING ${clang_full_version_string})
        if (VERBOSE)
            message (STATUS "The compiler is Clang: ${CMAKE_CXX_COMPILER_ID} version ${CLANG_VERSION_STRING}")
        endif ()
    endif ()
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    set (CMAKE_COMPILER_IS_INTEL 1)
    if (VERBOSE)
        message (STATUS "Using Intel as the compiler")
    endif ()
endif ()


###########################################################################
# Turn on more detailed warnings and optionally consider warnings as errors
#
if (NOT ${PROJECT_NAME}_SUPPORTED_RELEASE OR DEFINED ENV{${PROJECT_NAME}_CI})
    # For development branches (i.e., not a supported release), or when
    # running CI, default to treating all warnings as errors.
    option (STOP_ON_WARNING "Stop building if there are any compiler warnings" ON)
else ()
    # For release branches not doing a CI build default to just printing
    # warnings but not letting them stop the build.
    option (STOP_ON_WARNING "Stop building if there are any compiler warnings" OFF)
endif()
option (EXTRA_WARNINGS "Enable lots of extra pedantic warnings" OFF)
if (NOT MSVC)
    add_compile_options ("-Wall")
    if (EXTRA_WARNINGS)
        add_compile_options ("-Wextra")
    endif ()
    if (STOP_ON_WARNING)
        add_compile_options ("-Werror")
    endif ()
endif ()


###########################################################################
# Control symbol visibility
#
# We try hard to make default symbol visibility be "hidden", except for
# symbols that are part of the public API, which should be marked in the
# source code with a special decorator, OSL_API.
#
# Additionally, there is a hidesymbols.map file that on some platforms may
# give more fine-grained control for hiding symbols, because sometimes
# dependent libraries may not be well behaved and need extra hiding.
#
set (CMAKE_CXX_VISIBILITY_PRESET "hidden" CACHE STRING "Symbol visibility (hidden or default")
option (VISIBILITY_INLINES_HIDDEN "Hide symbol visibility of inline functions" ON)
set (VISIBILITY_MAP_FILE "${PROJECT_SOURCE_DIR}/src/build-scripts/hidesymbols.map" CACHE FILEPATH "Visibility map file")
set (CMAKE_C_VISIBILITY_PRESET ${CMAKE_CXX_VISIBILITY_PRESET})
if (${CMAKE_CXX_VISIBILITY_PRESET} STREQUAL "hidden" AND VISIBILITY_MAP_FILE AND
    (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG) AND
    (CMAKE_SYSTEM_NAME MATCHES "Linux|kFreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "GNU"))
    # Linux/FreeBSD/Hurd: also hide all the symbols of dependent libraries
    # to prevent clashes if an app using this project is linked against
    # other versions of our dependencies.
    set (VISIBILITY_MAP_COMMAND "-Wl,--version-script=${VISIBILITY_MAP_FILE}")
endif ()


###########################################################################
# Compiler-specific and platform-specific options.
#
# Here is where we add a whole bunch of options for specific compilers or
# platforms. Usually this is to suppress false-positive compiler warnings.
#
if (CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_APPLECLANG)
    # Clang-specific options
    add_compile_options ("-Wno-unused-function")
    add_compile_options ("-Wno-overloaded-virtual")
    add_compile_options ("-Wno-unneeded-internal-declaration")
    add_compile_options ("-Wno-unused-private-field")
    add_compile_options ("-Wno-tautological-compare")
    add_compile_options ("-Wno-unknown-pragmas")
    add_compile_options ("-Wno-format-security")
    # disable warning about unused command line arguments
    add_compile_options ("-Qunused-arguments")
    # Don't warn if we ask it not to warn about warnings it doesn't know
    add_compile_options ("-Wunknown-warning-option")
    if (CLANG_VERSION_STRING VERSION_GREATER_EQUAL 3.6 OR
        APPLECLANG_VERSION_STRING VERSION_GREATER 6.1)
        add_compile_options ("-Wno-unused-local-typedefs")
    endif ()
    if (CLANG_VERSION_STRING VERSION_GREATER_EQUAL 3.9)
        # Don't warn about using unknown preprocessor symbols in `#if`
        add_compile_options ("-Wno-expansion-to-defined")
    endif ()
    # Suppress warnings about our strategic use of bitwise operations in place
    # of logical operators to produce branchless code in some places.
    if (CLANG_VERSION_STRING VERSION_GREATER_EQUAL 14.0
        OR INTELCLANG_VERSION_STRING VERSION_GREATER_EQUAL 14.0)
        add_compile_options ("-Wno-bitwise-instead-of-logical")
    endif ()

    # disable warning in flex-generated code
    add_compile_options ("-Wno-null-conversion")
    add_compile_options ("-Wno-error=strict-overflow")
    if ($<CONFIG:Debug>)
        add_compile_options ("-Wno-unused-function -Wno-unused-variable")
    endif ()
endif ()

if (CMAKE_COMPILER_IS_GNUCC AND NOT (CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_APPLECLANG))
    add_compile_options ("-Wno-error=strict-overflow")
    add_compile_options ("-Wno-unused-local-typedefs")
    add_compile_options ("-Wno-unused-result")
    if (NOT ${GCC_VERSION} VERSION_LESS 6.0)
        add_compile_options ("-Wno-error=misleading-indentation")
    endif ()
endif ()

if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG)
    # Options common to gcc and clang

    # Ensure this macro is set for stdint.h
    add_definitions ("-D__STDC_LIMIT_MACROS")
    add_definitions ("-D__STDC_CONSTANT_MACROS")
    # this allows native instructions to be used for sqrtf instead of a function call
    add_compile_options ("-fno-math-errno")
endif ()

if (INTELCLANG_VERSION_STRING VERSION_GREATER_EQUAL 2022.1.0)
    # New versions of icx warn about changing certain floating point options
    add_compile_options ("-Wno-overriding-t-option")
endif ()

if (MSVC)
    # Microsoft specific options
    string (REGEX REPLACE "/W[1-4]" "/W1" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    add_definitions (-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS)
    add_definitions (-D_CRT_NONSTDC_NO_WARNINGS)
    add_definitions (-D_SCL_SECURE_NO_WARNINGS)
    add_definitions (-DJAS_WIN_MSVC_BUILD)
endif (MSVC)

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD"
    AND ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i386")
    # For FreeBSD, minimum arch of i586 is needed for atomic cpu instructions
    add_compile_options (-march=i586)
endif ()


# Remember the -std flags we need will be used later for custom Cuda builds
set (CSTD_FLAGS "")
if (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_INTEL)
    set (CSTD_FLAGS "-std=c++${CMAKE_CXX_STANDARD}")
endif ()



# We will use this for ccache and timing
set (MY_RULE_LAUNCH "")


###########################################################################
# Use ccache if found
#
# This can really speed up compilation by caching object files that have
# been compiled previously with identical arguments and inputs. Putting this
# logic here makes it work even if the user is unaware of ccache. If it's
# not found on the system, it will simply be silently not used.
option (USE_CCACHE "Use ccache if found" ON)
find_program (CCACHE_FOUND ccache)
if (CCACHE_FOUND AND USE_CCACHE)
    if (CMAKE_COMPILER_IS_CLANG AND USE_QT AND (NOT DEFINED ENV{CCACHE_CPP2}))
        message (STATUS "Ignoring ccache because clang + Qt + env CCACHE_CPP2 is not set")
    else ()
        set (MY_RULE_LAUNCH ccache)
    endif ()
endif ()


###########################################################################
# Build time debugging aid: time all compile & link commands
#
# Note, though, that this is not especially helpful when doing a parallel
# build. If you wish to time individual compile commands, it's best to also
# set `-j 1` or CMAKE_BUILD_PARALLEL_LEVEL to 1.
option (TIME_COMMANDS "Time each compile and link command" OFF)
if (TIME_COMMANDS)
    set (MY_RULE_LAUNCH "${CMAKE_COMMAND} -E time ${MY_RULE_LAUNCH}")
endif ()


# Note: This must be after any option that alters MY_RULE_LAUNCH
if (MY_RULE_LAUNCH)
    set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${MY_RULE_LAUNCH})
    set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK ${MY_RULE_LAUNCH})
endif ()


###########################################################################
# Option to force use of libc++ (the LLVM project's alternate C++ standard
# library). Currently this only has an effect if using clang as the
# compiler. Maybe it would also work for g++? Investigate.
option (USE_LIBCPLUSPLUS "Compile with clang libc++" OFF)
if (USE_LIBCPLUSPLUS AND CMAKE_COMPILER_IS_CLANG)
    message (STATUS "Using libc++")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()


###########################################################################
# For gcc >= 5, allow an option to force which version of the C++ ABI to
# use (mostly this affects the implementation of std::string).
#
# FIXME: In theory, this should also be needed for clang, if compiling with
# the gcc libstdc++ toolchain. In practice, I could not get things to build
# with clang properly when using this option, and I haven't yet seen a case
# where it's needed. We can return to this and fix for clang if it becomes a
# legit problem later.
#
set (GLIBCXX_USE_CXX11_ABI "" CACHE STRING "For gcc, use the new C++11 library ABI (0|1)")
if (CMAKE_COMPILER_IS_GNUCC AND ${GCC_VERSION} VERSION_GREATER_EQUAL 5.0)
    if (NOT ${GLIBCXX_USE_CXX11_ABI} STREQUAL "")
        add_definitions ("-D_GLIBCXX_USE_CXX11_ABI=${GLIBCXX_USE_CXX11_ABI}")
    endif ()
endif ()


###########################################################################
# SIMD and machine architecture options.
#
# The USE_SIMD option may be set to a comma-separated list of machine /
# instruction set options, such as "avx3,f16c". The list will be parsed and
# the proper compiler directives added to generate code for those ISA
# capabilities.
#
set (USE_SIMD "" CACHE STRING "Use SIMD directives (0, sse2, sse3, ssse3, sse4.1, sse4.2, avx, avx2, avx512f, f16c, aes)")
set (SIMD_COMPILE_FLAGS "")
if (NOT USE_SIMD STREQUAL "")
    message (STATUS "Compiling with SIMD level ${USE_SIMD}")
    if (USE_SIMD STREQUAL "0")
        set (SIMD_COMPILE_FLAGS ${SIMD_COMPILE_FLAGS} "-DOIIO_NO_SSE=1")
    else ()
        string (REPLACE "," ";" SIMD_FEATURE_LIST ${USE_SIMD})
        foreach (feature ${SIMD_FEATURE_LIST})
            if (VERBOSE)
                message (STATUS "SIMD feature: ${feature}")
            endif ()
            if (MSVC OR CMAKE_COMPILER_IS_INTEL)
                set (SIMD_COMPILE_FLAGS ${SIMD_COMPILE_FLAGS} "/arch:${feature}")
            else ()
                set (SIMD_COMPILE_FLAGS ${SIMD_COMPILE_FLAGS} "-m${feature}")
            endif ()
            if (feature STREQUAL "fma" AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG))
                # If fma is requested, for numerical accuracy sake, turn it
                # off by default except when we explicitly use madd. At some
                # future time, we should look at this again carefully and
                # see if we want to use it more widely by ffp-contract=fast.
                add_compile_options ("-ffp-contract=off")
            endif ()
        endforeach()
    endif ()
    add_compile_options (${SIMD_COMPILE_FLAGS})
endif ()

###########################################################################
# Batched SIMD shader execution options.
#
# The USE_BATCHED option may be set to indicate that support for batched
# SIMD shader execution be compiled along with targe specific libraries
set (USE_BATCHED "" CACHE STRING "Build batched SIMD shader execution for (0, b8_AVX, b8_AVX2, b8_AVX2_noFMA, b8_AVX512, b8_AVX512_noFMA, b16_AVX512, b16_AVX512_noFMA)")
option (VEC_REPORT "Enable compiler's reporting system for vectorization" OFF)
set (BATCHED_SUPPORT_DEFINES "")
set (BATCHED_TARGET_LIBS "")
set (OSL_BUILD_BATCHED 0)
if (NOT USE_BATCHED STREQUAL "")
    message (STATUS "Compiling with batched SIMD targets ${USE_BATCHED}")
    if (NOT USE_BATCHED STREQUAL "0")
        set (OSL_BUILD_BATCHED 1)
        string (REPLACE "," ";" BATCHED_TARGET_LIST ${USE_BATCHED})
        foreach (batched_target ${BATCHED_TARGET_LIST})
            if (VERBOSE)
                message (STATUS "BATCHED target: ${batched_target}")
            endif ()
            list (APPEND BATCHED_SUPPORT_DEFINES "__OSL_SUPPORTS_${batched_target}")
        endforeach()
    else ()
        message (STATUS "Compiling with NO batched SIMD targets, USE_BATCHED=0")
    endif ()
else ()
    message (STATUS "Compiling with NO batched SIMD targets, USE_BATCHED is empty")
endif ()

###########################################################################
# Preparation to test for compiler/language features
if (NOT VERBOSE)
    set (CMAKE_REQUIRED_QUIET 1)
endif ()
include (CMakePushCheckState)
include (CheckCXXSourceRuns)


###########################################################################
# Code coverage options
#
option (CODECOV "Build code coverage tests" OFF)
if (CODECOV AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG))
    message (STATUS "Compiling for code coverage analysis")
    add_compile_options (-ftest-coverage -fprofile-arcs)
    add_definitions ("-D${PROJ_NAME}_CODE_COVERAGE=1")
    link_libraries(gcov)
endif ()


###########################################################################
# Sanitizer options
#
set (SANITIZE "" CACHE STRING "Build code using sanitizer (address, thread)")
if (SANITIZE AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_CLANG))
    message (STATUS "Compiling for sanitizer=${SANITIZE}")
    string (REPLACE "," ";" SANITIZE_FEATURE_LIST ${SANITIZE})
    foreach (feature ${SANITIZE_FEATURE_LIST})
        message (STATUS "  sanitize feature: ${feature}")
        add_compile_options (-fsanitize=${feature})
        add_link_options (-fsanitize=${feature})
    endforeach()
    add_compile_options (-g -fno-omit-frame-pointer)
    if (CMAKE_COMPILER_IS_GNUCC)
        # turn on glibcxx extra annotations to find vector writes past end
        add_definitions ("-D_GLIBCXX_SANITIZE_VECTOR=1")
    endif ()
    add_definitions ("-D${PROJECT_NAME}_SANITIZE=1")
endif ()


###########################################################################
# clang-tidy options
#
# clang-tidy is a static analyzer that is part of the LLVM tools. It has a
# variety of the usual bug and security tests, linting, and also tests for
# things like finding (and correcting!) use of older language constructs.
#
# If clang-tidy is found and enabled, a "clang-tidy" build target will be
# enabled. The set of tests can be customized both here and through
# the .clang-tidy file that is part of this project.
#
option (CLANG_TIDY "Enable clang-tidy" OFF)
set (CLANG_TIDY_CHECKS "-*" CACHE STRING "clang-tidy checks to perform (none='-*')")
set (CLANG_TIDY_ARGS "" CACHE STRING "clang-tidy args")
option (CLANG_TIDY_FIX "Have clang-tidy fix source" OFF)
if (CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES "clang-tidy"
                 DOC "Path to clang-tidy executable")
    message (STATUS "CLANG_TIDY_EXE ${CLANG_TIDY_EXE}")
    if (CLANG_TIDY_EXE AND NOT ${CMAKE_VERSION} VERSION_LESS 3.6)
        set (CMAKE_CXX_CLANG_TIDY
             "${CLANG_TIDY_EXE}"
             )
        if (CLANG_TIDY_ARGS)
            list (APPEND CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_ARGS})
        endif ()
        if (CLANG_TIDY_CHECKS)
            list (APPEND CMAKE_CXX_CLANG_TIDY -checks="${CLANG_TIDY_CHECKS}")
        endif ()
        set (CMAKE_CXX_CLANG_TIDY "-header-filter=(${PROJECT_NAME})|(${PROJ_NAME})|(${PROJ_NAME_LOWER})|(_pvt.h)")
        execute_process (COMMAND ${CMAKE_CXX_CLANG_TIDY} -list-checks
                         OUTPUT_VARIABLE tidy_checks
                         OUTPUT_STRIP_TRAILING_WHITESPACE)
        if (CLANG_TIDY_FIX)
            list (APPEND CMAKE_CXX_CLANG_TIDY "-fix")
        endif ()
        message (STATUS "clang-tidy command line is: ${CMAKE_CXX_CLANG_TIDY}")
        message (STATUS "${tidy_checks}")
    else ()
        message (STATUS "Cannot run clang-tidy as requested")
    endif ()
    # Hint: run with CLANG_TIDY_ARGS=-list-checks to list all the checks
endif ()


###########################################################################
# clang-format options
#
# clang-format is a source code reformatter that is part of the LLVM tools.
# It can be used to check adherence to project code formatting rules and
# correct any deviations. If clang-format is found on the system, a
# "clang-format" build target will trigger a reformatting.
#
# Note: skip all of this checking, setup, and cmake-format target if this
# is being built as a subproject.
if (PROJECT_IS_TOP_LEVEL)
    set (CLANG_FORMAT_EXE_HINT "" CACHE PATH "clang-format executable's directory (will search if not specified")
    set (CLANG_FORMAT_INCLUDES  "src/*.cpp" "src/*.h" "src/*.cu"
        CACHE STRING "Glob patterns to include for clang-format")
    set (CLANG_FORMAT_EXCLUDES
            # Files "imported and modified" that we don't want to reformat, so
            # they continue to match their upstream versions.
            "src/include/OSL/Imathx/*"
            "src/include/OSL/matrix22.h"
            # Header files in testsuite are almost certainly osl headers, not C++
            "testsuite/*.h"
            # Header files in shaders are OSL, not C++
            "src/shaders/*.h"
         CACHE STRING "Glob patterns to exclude for clang-format")
    find_program (CLANG_FORMAT_EXE
                  NAMES clang-format bin/clang-format
                  HINTS ${CLANG_FORMAT_EXE_HINT} ENV CLANG_FORMAT_EXE_HINT
                        ENV LLVM_DIRECTORY
                  NO_DEFAULT_PATH
                  DOC "Path to clang-format executable")
    find_program (CLANG_FORMAT_EXE NAMES clang-format bin/clang-format)
    if (CLANG_FORMAT_EXE)
        message (STATUS "clang-format found: ${CLANG_FORMAT_EXE}")
        # Start with the list of files to include when formatting...
        file (GLOB_RECURSE FILES_TO_FORMAT ${CLANG_FORMAT_INCLUDES})
        # ... then process any list of excludes we are given
        foreach (_pat ${CLANG_FORMAT_EXCLUDES})
            file (GLOB_RECURSE _excl ${_pat})
            list (REMOVE_ITEM FILES_TO_FORMAT ${_excl})
        endforeach ()
        # message (STATUS "clang-format file list: ${FILES_TO_FORMAT}")
        file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/.clang-format
              DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
        add_custom_target (clang-format
            COMMAND "${CLANG_FORMAT_EXE}" -i -style=file ${FILES_TO_FORMAT} )
    else ()
        message (STATUS "clang-format not found.")
    endif ()
endif ()


###########################################################################
# Another way to sneak in custom compiler and DSO linking flags.
#
set (EXTRA_CPP_ARGS "" CACHE STRING "Extra C++ command line definitions")
if (EXTRA_CPP_ARGS)
    message (STATUS "Extra C++ args: ${EXTRA_CPP_ARGS}")
    add_compile_options ("${EXTRA_CPP_ARGS}")
endif()
set (EXTRA_DSO_LINK_ARGS "" CACHE STRING "Extra command line definitions when building DSOs")


###########################################################################
# Set the versioning for shared libraries.
#
if (${PROJECT_NAME}_SUPPORTED_RELEASE)
    # Supported releases guarantee ABI back-compatibility within the release
    # family, so SO versioning is major.minor.
    set (SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
         CACHE STRING "Set the SO version for dynamic libraries")
else ()
    # Development master makes no ABI stability guarantee, so we make the
    # SO naming capture down to the major.minor.patch level.
    set (SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
         CACHE STRING "Set the SO version for dynamic libraries")
endif ()
if (VERBOSE)
    message(STATUS "Setting SOVERSION to: ${SOVERSION}")
endif ()


###########################################################################
# BUILD_SHARED_LIBS, if turned off, will disable building of .so/.dll
# dynamic libraries and instead only build static libraries.
#
option (BUILD_SHARED_LIBS "Build shared libraries (set to OFF to build static libs)" ON)
if (NOT BUILD_SHARED_LIBS)
    add_definitions (-D${PROJECT_NAME}_STATIC_DEFINE=1)
endif ()


###########################################################################
# LINKSTATIC, if enabled, will cause us to favor linking static versions
# of library dependencies, if they are available.
#
option (LINKSTATIC  "Link with static external libraries when possible" OFF)
if (LINKSTATIC)
    #set (_orig_link_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
    message (STATUS "Statically linking external libraries when possible")
    if (WIN32)
        set (CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    else ()
        set (CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    endif ()
endif ()


###########################################################################
# Any extra logic to be run only for CI builds goes here.
# We expect our own CI runs to define env variable ${PROJECT_NAME}_CI
#
if (DEFINED ENV{${PROJECT_NAME}_CI})
    add_definitions (-D${PROJ_NAME}_CI=1 -DBUILD_CI=1)
    if (APPLE)
        # Keep Mono framework from being incorrectly searched for include
        # files on GitHub Actions CI.
        set(CMAKE_FIND_FRAMEWORK LAST)
    endif ()
endif ()


###########################################################################
# Rpath handling at the install step
#
set (MACOSX_RPATH ON)
if (CMAKE_SKIP_RPATH)
    # We need to disallow the user from truly setting CMAKE_SKIP_RPATH, since
    # we want to run the generated executables from the build tree in order to
    # generate the manual page documentation.  However, we make sure the
    # install rpath is unset so that the install tree is still free of rpaths
    # for linux packaging purposes.
    set (CMAKE_SKIP_RPATH FALSE)
    unset (CMAKE_INSTALL_RPATH)
else ()
    if (NOT CMAKE_INSTALL_RPATH)
        set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_FULL_LIBDIR}")
    endif ()
    # add the automatically determined parts of the RPATH that
    # point to directories outside the build tree to the install RPATH
    set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    if (VERBOSE)
        message (STATUS "CMAKE_INSTALL_RPATH = ${CMAKE_INSTALL_RPATH}")
    endif ()
endif ()
# OSL considerations because we run oslc in the course of building:
# Use (i.e. don't skip) the full RPATH for the build tree
set (CMAKE_SKIP_BUILD_RPATH  FALSE)
# When building, don't use the install RPATH already (but later on when installing)
set (CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)


###########################################################################
# Macro to install targets to the appropriate locations.  Use this instead
# of the install(TARGETS ...) signature. Note that it adds it to the
# export targets list for when we generate config files.
#
# Usage:
#
#    install_targets (target1 [target2 ...])
#
macro (install_targets)
    install (TARGETS ${ARGN}
             EXPORT ${PROJ_NAME}_EXPORTED_TARGETS
             RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT user
             LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT user
             ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT developer)
endmacro()
