# Create executable for every *.cpp file
add_custom_target(examples)

# FILE(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
# FOREACH(child ${children})
#     IF(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${child})
#         add_subdirectory(${child})
#     ENDIF()
# ENDFOREACH()

add_executable(
    minimal_example

    minimal_example.cpp
)

target_include_directories(minimal_example INTERFACE include)

target_link_libraries(
    minimal_example

    compact_sparse_hash
)
add_dependencies(examples minimal_example)

