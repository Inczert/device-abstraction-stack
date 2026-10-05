cmake_minimum_required(VERSION 3.20)
project(das_package_contract LANGUAGES NONE)

set(DAS_REQUIRED_VERSION "0.1" CACHE STRING "DAS version requested by this contract test")
set(DAS_EXPECTED_VERSION "0.1.0" CACHE STRING "Expected installed DAS package version")
set(DAS_EXPECTED_DEVICE "nucleo_h755zi_q" CACHE STRING "Expected installed DAS device")
set(DAS_EXPECTED_CORE "" CACHE STRING "Expected installed DAS core")

if(NOT DAS_EXPECTED_CORE)
    message(FATAL_ERROR "DAS_EXPECTED_CORE must be set")
endif()

find_package(DAS ${DAS_REQUIRED_VERSION} CONFIG REQUIRED)
find_package(DAS ${DAS_REQUIRED_VERSION} CONFIG REQUIRED)

if(NOT TARGET das::das)
    message(FATAL_ERROR "Installed DAS package did not export das::das")
endif()
if(NOT DAS_VERSION STREQUAL DAS_EXPECTED_VERSION)
    message(FATAL_ERROR "Unexpected DAS version '${DAS_VERSION}', expected '${DAS_EXPECTED_VERSION}'")
endif()
if(NOT DAS_PACKAGE_VERSION STREQUAL DAS_EXPECTED_VERSION)
    message(FATAL_ERROR "Unexpected DAS package version '${DAS_PACKAGE_VERSION}'")
endif()
if(NOT DAS_PACKAGE_DEVICE STREQUAL DAS_EXPECTED_DEVICE)
    message(FATAL_ERROR "Unexpected DAS package device '${DAS_PACKAGE_DEVICE}'")
endif()
if(NOT DAS_PACKAGE_CORE STREQUAL DAS_EXPECTED_CORE)
    message(FATAL_ERROR "Unexpected DAS package core '${DAS_PACKAGE_CORE}'")
endif()
if(NOT EXISTS "${DAS_PACKAGE_LINKER_SCRIPT}")
    message(FATAL_ERROR "Installed DAS linker script is missing: ${DAS_PACKAGE_LINKER_SCRIPT}")
endif()

get_target_property(_das_link_options das::das INTERFACE_LINK_OPTIONS)
get_target_property(_das_link_depends das::das INTERFACE_LINK_DEPENDS)

set(_das_linker_option "-T${DAS_PACKAGE_LINKER_SCRIPT}")
set(_das_vector_option "-Wl,-u,g_das_vector_table")
set(_das_linker_count 0)
set(_das_vector_count 0)

foreach(_option IN LISTS _das_link_options)
    if(_option STREQUAL _das_linker_option)
        math(EXPR _das_linker_count "${_das_linker_count} + 1")
    endif()
    if(_option STREQUAL _das_vector_option)
        math(EXPR _das_vector_count "${_das_vector_count} + 1")
    endif()
endforeach()

if(NOT _das_linker_count EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one installed linker option, found ${_das_linker_count}: ${_das_link_options}")
endif()
if(NOT _das_vector_count EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one default-vector force-link option, found ${_das_vector_count}: ${_das_link_options}")
endif()

set(_das_link_depends_count 0)
foreach(_dependency IN LISTS _das_link_depends)
    if(_dependency STREQUAL DAS_PACKAGE_LINKER_SCRIPT)
        math(EXPR _das_link_depends_count "${_das_link_depends_count} + 1")
    endif()
endforeach()
if(NOT _das_link_depends_count EQUAL 1)
    message(FATAL_ERROR
        "Expected exactly one linker dependency, found ${_das_link_depends_count}: ${_das_link_depends}")
endif()

message(STATUS
    "DAS package contract PASS: version=${DAS_VERSION}, device=${DAS_PACKAGE_DEVICE}, "
    "core=${DAS_PACKAGE_CORE}, linker=${DAS_PACKAGE_LINKER_SCRIPT}")
