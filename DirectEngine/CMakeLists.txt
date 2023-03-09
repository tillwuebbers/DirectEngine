# Main CMakeLists.txt
cmake_minimum_required (VERSION 3.12)

# dirs
set(XAUDIO2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/xaudio2)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(DX_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/directx/headers)
set(GLM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glm)
set(METADESK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/metadesk/source)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(REACTPHYSICS_DIR "E:/Code/ReactPhysics3DInstallDebug")
else()
    set(REACTPHYSICS_DIR "E:/Code/ReactPhysics3DInstallRelease")
endif()

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")

include_directories("${REACTPHYSICS_DIR}/include")
list(APPEND CMAKE_PREFIX_PATH ${REACTPHYSICS_DIR})
find_package(reactphysics3d CONFIG REQUIRED)

# code groups
file (GLOB CORE_CODE core/*.h core/*.cpp)
file (GLOB GAME_CODE game/*.h game/*.cpp)
file (GLOB IMPORT_CODE "imgui/remixicon.h"
                "imgui/ImGuiProfilerRenderer.h"
                "imgui/ProfilerTask.h"
                "import/tiny_gltf.h"
                "import/json.hpp"
                "import/stb_image.h"
                "import/stb_image_write.h"
                "directx-tex/DDSTextureLoader12.h"
                "directx-tex/DDSTextureLoader12.cpp")
file (GLOB SHADER_CODE "shaders/compile/*.hlsl")

function(DEPS PROJ)
    # imgui
    target_include_directories(${PROJ}
                                PRIVATE ${IMGUI_DIR}
                                PRIVATE ${IMGUI_DIR}/backends
                                PRIVATE ${GLM_DIR}
                                PRIVATE ${METADESK_DIR})

    target_sources(${PROJ}
                    PRIVATE
                        ${IMGUI_DIR}/imgui_demo.cpp
                        ${IMGUI_DIR}/imgui_draw.cpp
                        ${IMGUI_DIR}/imgui_tables.cpp
                        ${IMGUI_DIR}/imgui_widgets.cpp
                        ${IMGUI_DIR}/imgui.cpp
                        ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
                        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp)

    # directx
    target_include_directories(${PROJ} PRIVATE ${DX_HEADER_DIR})

    # xaudio2
    target_include_directories(${PROJ} PRIVATE ${XAUDIO2_DIR}/include)

    # libs
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        file(COPY "${XAUDIO2_DIR}/debug/bin/x64/xaudio2_9redist.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
        target_link_libraries(${PROJ} "${XAUDIO2_DIR}/debug/lib/x64/xaudio2_9redist.lib")
    else()
        file(COPY "${XAUDIO2_DIR}/release/bin/x64/xaudio2_9redist.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
        target_link_libraries(${PROJ} "${XAUDIO2_DIR}/release/lib/x64/xaudio2_9redist.lib")
    endif()
    target_link_libraries(${PROJ} "d3d12.lib" "dxgi.lib" "dxguid.lib" "d3dcompiler.lib")
    
    # options
    if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_link_options(${PROJ} PRIVATE $<$<CONFIG:RELWITHDEBINFO>:/PROFILE>)
    endif()
    target_compile_definitions(${PROJ} PRIVATE "UNICODE;_UNICODE")
    target_compile_options(${PROJ} PUBLIC -std:c++20 /Zc:strictStrings-)
endfunction()

# executable
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(${PROJECT_NAME} WIN32 "DirectEngine.cpp" "DirectEngine.h")
target_link_libraries(${PROJECT_NAME} ${CORE_NAME})
DEPS(${PROJECT_NAME})

# assets
file(COPY "Montserrat-Regular.ttf" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY "remixicon.ttf" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
cmake_path(NATIVE_PATH SHADER_DIR SHADER_DIR_NATIVE)
set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_bin")

add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/audio
                ${CMAKE_CURRENT_BINARY_DIR}/audio)

add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${SHADER_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/models
                ${CMAKE_CURRENT_BINARY_DIR}/models)
                
add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/textures
                ${CMAKE_CURRENT_BINARY_DIR}/textures)
      
# sub projects
add_subdirectory(core)
add_subdirectory(game)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_subdirectory(tests)
endif()
