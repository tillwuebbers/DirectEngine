# Core
cmake_minimum_required (VERSION 3.12)

file (GLOB CORE_CODE *.h *.cpp)
file (GLOB IMPORT_CODE "../imgui/remixicon.h"
                "../imgui/ImGuiProfilerRenderer.h"
                "../imgui/ProfilerTask.h"
                "../import/tiny_gltf.h"
                "../import/json.hpp"
                "../import/stb_image.h"
                "../import/stb_image_write.h"
                "../directx-tex/DDSTextureLoader12.h"
                "../directx-tex/DDSTextureLoader12.cpp")
file (GLOB SHADER_CODE "${CMAKE_CURRENT_SOURCE_DIR}/../shaders/compile/*.hlsl")

# Add source to this project's executable.
add_library (${PROJECT_NAME}
                "../Helpers.h"
                ${CORE_CODE}
                ${IMPORT_CODE}
                ${SHADER_CODE})

DEPS(${PROJECT_NAME})

# custom flags
target_compile_definitions(${PROJECT_NAME} PRIVATE "ENABLE_TIMERS")
