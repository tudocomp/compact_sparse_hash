add_custom_target(examples)

# Create executable for every *.cpp file
FILE(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.cpp)
FOREACH(child ${children})
    get_filename_component(executable ${child} NAME_WE)
    
    add_executable(
        ${executable} 

        ${child}
        )

    target_include_directories(${executable} INTERFACE include)

    target_link_libraries(
        ${executable}

        compact_sparse_hash
        )
    add_dependencies(examples ${executable})

ENDFOREACH()
