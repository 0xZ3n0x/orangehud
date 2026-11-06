cmake_minimum_required(VERSION 3.21)
file(STRINGS VERSION ORANGEHUD_VERSION LIMIT_COUNT 1)
project(OrangeHUD VERSION ${ORANGEHUD_VERSION} LANGUAGES C CXX)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

option(ORANGEHUD_BUILD_TESTS "Configure the host unit tests (host compiler, no PS5 SDK) instead of the payload" OFF)
option(ORANGEHUD_LOGGING "Compile klog logging (LOG / LOG_*)" OFF)
if(ORANGEHUD_LOGGING)
	add_compile_definitions(LOGGING)
endif()
add_compile_definitions(ORANGEHUD_VERSION="${PROJECT_VERSION}")

set(ORANGE_SRC "${PROJECT_SOURCE_DIR}/src")
set(ORANGE_THIRD_PARTY "${PROJECT_SOURCE_DIR}/third_party")
include_directories("${ORANGE_SRC}/shared" "${ORANGE_THIRD_PARTY}")

if(NOT ORANGEHUD_BUILD_TESTS)
	include(payload_sdk)
endif()

include(orange_targets)

add_subdirectory(src)
if(ORANGEHUD_BUILD_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()
