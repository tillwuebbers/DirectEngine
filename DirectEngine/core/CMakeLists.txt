# Core
project(${CORE_NAME})

# Add source to this project's executable.
add_library (${PROJECT_NAME}
                "../Helpers.h"
                ${CORE_CODE}
                ${IMPORT_CODE}
                ${SHADER_CODE}
                ${SHADER_CODE_RT}
                ${SHADER_CODE_CS})

set_source_files_properties(${SHADER_CODE} ${SHADER_CODE_RT} ${SHADER_CODE_CS} ${CORE_CODE} PROPERTIES VS_TOOL_OVERRIDE "None")

source_group("Rasterize Shader Files" FILES ${SHADER_CODE})
source_group("Raytrace Shader Files" FILES ${SHADER_CODE_RT})
source_group("Compute Shader Files" FILES ${SHADER_CODE_CS})
source_group("Code" FILES ${CORE_CODE})

DEPS(${PROJECT_NAME})

# compile shaders
target_include_directories(${PROJECT_NAME} PRIVATE "${SHADER_DIR}")
foreach(SHADER_FILE ${SHADER_CODE})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)

    add_custom_command(
        OUTPUT "${SHADER_OUT_DIR}/${SHADER_NAME}.vert.cso" "${SHADER_OUT_DIR}/${SHADER_NAME}.frag.cso"
        COMMAND echo ${SHADER_NAME} "including" ${SHADER_DIR_NATIVE} && dxc /T vs_6_0 /E "VSMain" /Fo "${SHADER_OUT_DIR}/${SHADER_NAME}.vert.cso" /I ${SHADER_DIR_NATIVE} "${SHADER_FILE}" && dxc /T ps_6_0 /E "PSMain" /Fo "${SHADER_OUT_DIR}/${SHADER_NAME}.frag.cso" /I ${SHADER_DIR_NATIVE} "${SHADER_FILE}"
        MAIN_DEPENDENCY "${SHADER_FILE}"
        DEPENDS "${SHADER_DIR}/util/common.hlsl"
    )
endforeach()

foreach(SHADER_FILE_RT ${SHADER_CODE_RT})
    get_filename_component(SHADER_NAME_RT ${SHADER_FILE_RT} NAME_WE)

    add_custom_command(
        OUTPUT "${SHADER_OUT_DIR}/${SHADER_NAME_RT}.rt.cso"
        COMMAND echo ${SHADER_NAME_RT} "including" ${SHADER_DIR_NATIVE} && dxc /T lib_6_3 /Fo "${SHADER_OUT_DIR}/${SHADER_NAME_RT}.rt.cso" /I ${SHADER_DIR_NATIVE} "${SHADER_FILE_RT}"
        MAIN_DEPENDENCY "${SHADER_FILE_RT}"
        DEPENDS "${SHADER_DIR}/util/common.hlsl"
    )
endforeach()

foreach(SHADER_FILE_CS ${SHADER_CODE_CS})
    get_filename_component(SHADER_NAME_CS ${SHADER_FILE_CS} NAME_WE)

    add_custom_command(
        OUTPUT "${SHADER_OUT_DIR}/${SHADER_NAME_CS}.cs.cso"
        COMMAND echo ${SHADER_NAME_CS} "including" ${SHADER_DIR_NATIVE} && dxc /T cs_6_0 /Fo "${SHADER_OUT_DIR}/${SHADER_NAME_CS}.cs.cso" /I ${SHADER_DIR_NATIVE} "${SHADER_FILE_CS}"
        MAIN_DEPENDENCY "${SHADER_FILE_CS}"
        DEPENDS "${SHADER_DIR}/util/common.hlsl"
    )
endforeach()

# custom flags
target_compile_definitions(${PROJECT_NAME} PRIVATE "ENABLE_TIMERS" "COMPILING_CORE")
