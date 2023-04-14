# Core
project(${CORE_NAME})

# Add source to this project's executable.
add_library (${PROJECT_NAME}
                "../Helpers.h"
                ${CORE_CODE}
                ${IMPORT_CODE}
                ${SHADER_CODE})

source_group("Shader Files" FILES ${SHADER_CODE})

DEPS(${PROJECT_NAME})
   
# compile shaders
target_include_directories(${PROJECT_NAME} PRIVATE "${SHADER_DIR}")
foreach(SHADER_FILE ${SHADER_CODE})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)

    add_custom_command(
        OUTPUT "${SHADER_OUT_DIR}/${SHADER_NAME}.vert.cso" "${SHADER_OUT_DIR}/${SHADER_NAME}.frag.cso"
        COMMAND dxc /T vs_6_0 /E "VSMain" /Fo "${SHADER_OUT_DIR}/${SHADER_NAME}.vert.cso" /I ${SHADER_DIR_NATIVE} "${SHADER_FILE}" && dxc /T ps_6_0 /E "PSMain" /Fo "${SHADER_OUT_DIR}/${SHADER_NAME}.frag.cso" /I ${SHADER_DIR_NATIVE} "${SHADER_FILE}"
        MAIN_DEPENDENCY "${SHADER_FILE}"
    )
endforeach()


# custom flags
target_compile_definitions(${PROJECT_NAME} PRIVATE "ENABLE_TIMERS")
