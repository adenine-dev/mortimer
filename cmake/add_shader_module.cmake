function(add_shader_modules TARGET)
    find_program(GLSLC glslc)
    
    foreach(_source IN ITEMS ${ARGN})
        file(RELATIVE_PATH relpath ${CMAKE_SOURCE_DIR} ${_source})

        set(input_file ${_source})
        set(spv_output_file ${CMAKE_BINARY_DIR}/shaders/${relpath}.spv)

        get_filename_component(current_output_dir ${spv_output_file} DIRECTORY)
        file(MAKE_DIRECTORY ${current_output_dir})

        get_filename_component(current_dir ${_source} DIRECTORY)
        file(GLOB_RECURSE common_glsl ${current_dir}/common/*.glsl)
        add_custom_command(
            OUTPUT ${spv_output_file}
            COMMAND ${GLSLC} -g -O0 -o ${spv_output_file} ${input_file}
            MAIN_DEPENDENCY ${input_file}
            IMPLICIT_DEPENDS C ${input_file}
            # FIXME: this is bad but for our purposes is good enough, this 
            # causes all shader files to be rebuilt any time any of the common 
            # files are changed even if they don't include them
            IMPLICIT_DEPENDS ${common_glsl}
            VERBATIM)

        set_source_files_properties(${spv_output_file} PROPERTIES GENERATED TRUE)

        set(outdir ${current_dir}/embed)
        target_add_binary_embed(${TARGET} ${spv_output_file} OUTDIR ${outdir} ALIGNMENT 4)
    endforeach()

endfunction(add_shader_modules)
