cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

project (compact_sparse_hash)

# Check if this project is build standalone
#
# We do this in case we want to use this repo as a GIT submodule,
# because then we only need the source files themselves
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # I am top-level project.

    find_program(CCACHE_FOUND ccache)
    if(CCACHE_FOUND)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
        set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    endif(CCACHE_FOUND)

    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif(NOT CMAKE_BUILD_TYPE)

    # Check compiler version
    if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU" AND
    ${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "5.4")
        message(FATAL_ERROR "g++ version 5.4 or greater required!")
    endif()

    set(CXX_STANDARD c++14)

    if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        # using Clang or AppleClang
        if(${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "3.4")
            message(FATAL_ERROR "clang version 3.4 or greater required!")
        endif()
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # using GCC
        if(${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS "5.4")
            message(FATAL_ERROR "g++ version 5.4 or greater required!")
        endif()
    else()
        message(FATAL_ERROR "Compiler ${CMAKE_CXX_COMPILER_ID} is not supported!")
    endif()

    include(ExternalProject)

    # More warnings and debug info
    # TODO: Add these, they break building glog -Wextra -Wpedantic
    set(GCC_WARNINGS "-Wall -Werror=return-type")
    #set(SANITIZATION "-fsanitize=undefined -fsanitize=address -fsanitize=leak")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14 -fdiagnostics-color=auto ${GCC_WARNINGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb -DDEBUG ${SANITIZATION}")

    # Global dependencies
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmakemodules")

    # Git Submodule sources
    include(git_submodule_subdirectories)

    # Finders
    include(find_or_download_package)

    find_or_download_package(Glog GLOG glog)
    git_submodule_subdirectory(external/bit_span)
endif()

# Main target
add_library(compact_sparse_hash INTERFACE)
target_link_libraries(compact_sparse_hash INTERFACE bit_span)
target_include_directories(compact_sparse_hash INTERFACE include)

# Check if this project is build standalone
#
# We do this in case we want to use this repo as a GIT submodule,
# because then we only need the source files themselves
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    # Unit tests for everything in src
    add_subdirectory(test)

    # Examples
    add_subdirectory(examples)

    MESSAGE( STATUS "Built Type: " ${CMAKE_BUILD_TYPE} )
endif()
