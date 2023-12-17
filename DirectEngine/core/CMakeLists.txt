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
source_group("Other Shader Files" FILES ${SHADER_CODE_UTIL})
source_group("Code" FILES ${CORE_CODE})

DEPS(${PROJECT_NAME})

# show but don't compile other shader files
target_sources(${PROJECT_NAME} PRIVATE ${SHADER_CODE_UTIL})
set_source_files_properties(${SHADER_CODE_UTIL} PROPERTIES HEADER_FILE_ONLY TRUE)

# custom flags
target_compile_definitions(${PROJECT_NAME} PRIVATE "ENABLE_TIMERS" "COMPILING_CORE")
