cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)

project (compact_sparse_hash)

# Check if this project is build standalone
#
# We do this in case we want to use this repo as a GIT submodule,
# because then we only need the source files themselves
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(CSH_STANDALONE 1)
endif()

if(CSH_STANDALONE)
    # init build system
    execute_process(COMMAND git submodule update --init -- build_system
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/submodules)

    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/submodules/build_system/cmakemodules")
    include(tdc_init)

    # downloadable dependencies
    include(depend_glog)

    # quit if dependencies aren't met
    tdc_check_hard_deps()
    if(TDC_DEPS_MISSING)
        return()
    endif()

    # soft dependencies
    include(softdepend_gtest)

    # submodules
    include(git_submodule_subdirectories)
    git_submodule_subdirectory(submodules/build_system)
    git_submodule_subdirectory(submodules/bit_span)
endif()

# Main target
add_library(compact_sparse_hash INTERFACE)
target_link_libraries(compact_sparse_hash INTERFACE bit_span)
target_include_directories(compact_sparse_hash INTERFACE include)

if(CSH_STANDALONE)
    # Unit tests
    add_subdirectory(test)

    # Examples
    add_subdirectory(examples)

    # Disclaimer
    MESSAGE(STATUS "Built Type: " ${CMAKE_BUILD_TYPE} )
endif()
