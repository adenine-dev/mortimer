function(add_shader_modules TARGET)
    find_program(GLSLC glslc)
    
    foreach(_source IN ITEMS ${ARGN})
        file(RELATIVE_PATH relpath ${CMAKE_SOURCE_DIR} ${_source})

        set(input_file ${_source})
        set(spv_output_file ${CMAKE_BINARY_DIR}/shaders/${relpath}.spv)

        get_filename_component(current_output_dir ${spv_output_file} DIRECTORY)
        file(MAKE_DIRECTORY ${current_output_dir})

        add_custom_command(
            OUTPUT ${spv_output_file}
            COMMAND ${GLSLC} -g -O0 -o ${spv_output_file} ${input_file}
            MAIN_DEPENDENCY ${input_file}
            IMPLICIT_DEPENDS CXX ${input_file}
            VERBATIM)

        set_source_files_properties(${spv_output_file} PROPERTIES GENERATED TRUE)

        get_filename_component(outdir ${_source} DIRECTORY)
        target_add_binary_embed(${TARGET} ${spv_output_file} OUTDIR ${outdir} ALIGNMENT 4)
    endforeach()

endfunction(add_shader_modules)
