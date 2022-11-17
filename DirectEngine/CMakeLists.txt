# CMakeList.txt : CMake project for DirectEngine, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)


# Add source to this project's executable.
add_executable (${PROJECT_NAME} WIN32
                "DirectEngine.cpp"
                "DirectEngine.h"
                "Helpers.h"
                "core/Coroutine.h"
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
                "import/tiny_obj_loader.h"
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

# dxtk
set(DXTK_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dxtk)

target_include_directories(${PROJECT_NAME}
                            PUBLIC ${DXTK_DIR}/Inc)

# directx
target_include_directories(${PROJECT_NAME} PRIVATE directx/headers)

# build the thing
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(${PROJECT_NAME} "d3d12.lib" "dxgi.lib" "dxguid.lib" "d3dcompiler.lib" "${DXTK_DIR}/Bin/Desktop_2022_Win10/x64/Debug/DirectXTK12.lib")
else()
    target_link_libraries(${PROJECT_NAME} "d3d12.lib" "dxgi.lib" "dxguid.lib" "d3dcompiler.lib" "${DXTK_DIR}/Bin/Desktop_2022_Win10/x64/Release/DirectXTK12.lib")
endif()


# assets
file(COPY "Montserrat-Regular.ttf" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY "remixicon.ttf" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

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
