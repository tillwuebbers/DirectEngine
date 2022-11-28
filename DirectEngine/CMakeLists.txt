# CMakeList.txt : CMake project for DirectEngine, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.12)


# Add source to this project's executable.
add_executable (${PROJECT_NAME} WIN32
                "DirectEngine.cpp"
                "DirectEngine.h"
                "Helpers.h"
                "core/Audio.h"
                "core/Audio.cpp"
                "core/Coroutine.h"
                "core/Common.h"
                "core/ComStack.cpp"
                "core/ComStack.h"
                "core/Constants.h"
                "core/DDS.h"
                "core/EngineCore.cpp"
                "core/EngineCore.h"
                "core/Input.h"
                "core/Input.cpp"
                "core/Memory.h"
                "core/Memory.cpp"
                "core/ShadowMap.cpp"
                "core/ShadowMap.h"
                "core/Texture.h"
                "core/Texture.cpp"
                "core/UI.cpp"
                "core/UI.h"
                "core/vkcodes.h"
                "imgui/remixicon.h"
                "imgui/ImGuiProfilerRenderer.h"
                "imgui/ProfilerTask.h"
                
                "import/tiny_gltf.h"
                "import/json.hpp"
                "import/stb_image.h"
                "import/stb_image_write.h"
                "directx-tex/DDSTextureLoader12.h"
                "directx-tex/DDSTextureLoader12.cpp"
                "game/Entity.h"
                "game/Entity.cpp"
                "game/IGame.h"
                "game/Game.h"
                "game/Game.cpp"
                "game/GameUI.cpp"
                "game/Puzzle.cpp"
                "game/Puzzle.h"
                "game/Log.h"
                "game/Log.cpp"
                "game/Mesh.h"
                "game/Mesh.cpp")


# imgui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

target_sources( ${PROJECT_NAME}
                PRIVATE
                    ${IMGUI_DIR}/imgui_demo.cpp
                    ${IMGUI_DIR}/imgui_draw.cpp
                    ${IMGUI_DIR}/imgui_tables.cpp
                    ${IMGUI_DIR}/imgui_widgets.cpp
                    ${IMGUI_DIR}/imgui.cpp

                PRIVATE
                    ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp
                    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
                )

target_include_directories( ${PROJECT_NAME}
                            PUBLIC ${IMGUI_DIR}
                            PUBLIC ${IMGUI_DIR}/backends
                            PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/glm )

# directx
target_include_directories(${PROJECT_NAME} PRIVATE directx/headers)

# xaudio2
set(XAUDIO2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/xaudio2)

target_include_directories(${PROJECT_NAME} PUBLIC ${XAUDIO2_DIR}/include)

# build the thing
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(COPY "${XAUDIO2_DIR}/debug/bin/x64/xaudio2_9redist.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${PROJECT_NAME} "d3d12.lib" "dxgi.lib" "dxguid.lib" "d3dcompiler.lib" "${XAUDIO2_DIR}/debug/lib/x64/xaudio2_9redist.lib")
else()
    file(COPY "${XAUDIO2_DIR}/release/bin/x64/xaudio2_9redist.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${PROJECT_NAME} "d3d12.lib" "dxgi.lib" "dxguid.lib" "d3dcompiler.lib" "${XAUDIO2_DIR}/release/lib/x64/xaudio2_9redist.lib")
endif()
target_compile_options(${PROJECT_NAME} PRIVATE /W3 /w34456 /w34189 /w44305 /w44244 /w44267)

# assets
file(COPY "Montserrat-Regular.ttf" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY "remixicon.ttf" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/audio
                ${CMAKE_CURRENT_BINARY_DIR}/audio)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders
                ${CMAKE_CURRENT_BINARY_DIR}/shaders)

add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/models
                ${CMAKE_CURRENT_BINARY_DIR}/models)
                
add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/textures
                ${CMAKE_CURRENT_BINARY_DIR}/textures)
                
# idk
target_compile_definitions(${PROJECT_NAME} PRIVATE "UNICODE;_UNICODE")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET DirectEngine PROPERTY CXX_STANDARD 20)
endif()
