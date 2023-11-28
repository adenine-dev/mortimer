function(target_add_binary_embed target filename)
    get_filename_component(base_filename ${filename} NAME)
    
    set(alignment 1)
    get_filename_component(pathname ${filename} DIRECTORY)

    math(EXPR last "${ARGC} - 1")
    foreach(n RANGE 0 ${last})
        if (${ARGV${n}} STREQUAL "ALIGNMENT")
            math(EXPR n "${n} + 1")
            set(alignment ${ARGV${n}})
        elseif(${ARGV${n}} STREQUAL "OUTDIR")
            math(EXPR n "${n} + 1")
            set(pathname ${ARGV${n}})
        endif()
    endforeach()

    string(MAKE_C_IDENTIFIER ${base_filename} c_name)

    add_custom_command(
        OUTPUT ${pathname}/${c_name}.h ${pathname}/${c_name}.c
        COMMAND ${CMAKE_COMMAND} -DRUN_CREATE_BINARY_EMBED=1 -DGENERATE_ALIGNMENT=${alignment} -DGENERATE_FILENAME=${filename} -DGENERATE_FILEPATH=${pathname} -P ${CMAKE_SOURCE_DIR}/cmake/target_add_binary_embed.cmake
        MAIN_DEPENDENCY ${filename}
        VERBATIM
    )

    target_sources(${target} PRIVATE ${pathname}/${c_name}.h ${pathname}/${c_name}.c)

    set_source_files_properties(${pathname}/${c_name}.h PROPERTIES GENERATED TRUE)
    set_source_files_properties(${pathname}/${c_name}.c PROPERTIES GENERATED TRUE)
endfunction(target_add_binary_embed)

# you should use nothing below this.
function(create_binary_embed filename pathname alignment)
    get_filename_component(base_filename ${filename} NAME)
    string(MAKE_C_IDENTIFIER ${base_filename} c_name)

    file(READ ${filename} content HEX)
    string(REGEX REPLACE "([A-Fa-f0-9][A-Fa-f0-9])" "0x\\0," output_c ${content})

    set(output_c "// This file was automatically generated and should not be manually modified.

#include \"${c_name}.h\"

_Alignas(${alignment}) const unsigned char ${c_name}_data[] = {
    ${output_c}
}\;
const size_t ${c_name}_size = sizeof(${c_name}_data)\;")

    set(output_h "// This file was automatically generated and should not be manually modified.

#pragma once
#include <stddef.h>

_Alignas(${alignment}) extern const unsigned char ${c_name}_data[]\;
extern const size_t ${c_name}_size\;")

    file(WRITE ${pathname}/${c_name}.c ${output_c})
    file(WRITE ${pathname}/${c_name}.h ${output_h})

endfunction(create_binary_embed)


if (RUN_CREATE_BINARY_EMBED)
    create_binary_embed(${GENERATE_FILENAME} ${GENERATE_FILEPATH} ${GENERATE_ALIGNMENT})
endif ()
