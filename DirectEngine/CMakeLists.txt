# CMakeList.txt : CMake project for DirectEngine, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.12)

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
file (GLOB SHADER_CODE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile/*.hlsl")

# Add source to this project's executable.
add_executable (${PROJECT_NAME} WIN32
                "DirectEngine.cpp"
                "DirectEngine.h"
                "Helpers.h"
                ${CORE_CODE}
                ${GAME_CODE}
                ${IMPORT_CODE}
                ${SHADER_CODE})

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

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(COPY "${XAUDIO2_DIR}/debug/bin/x64/xaudio2_9redist.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${PROJECT_NAME} "${XAUDIO2_DIR}/debug/lib/x64/xaudio2_9redist.lib")
else()
    file(COPY "${XAUDIO2_DIR}/release/bin/x64/xaudio2_9redist.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${PROJECT_NAME} "${XAUDIO2_DIR}/release/lib/x64/xaudio2_9redist.lib")
endif()

# edit and continue mode
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI")
  set(CMAKE_SHARED_LINKER_FLAGS "/SAFESEH:NO")
  set(CMAKE_EXE_LINKER_FLAGS "/SAFESEH:NO")
endif()

# build the thing
target_link_libraries(${PROJECT_NAME} "d3d12.lib" "dxgi.lib" "dxguid.lib" "d3dcompiler.lib")
target_compile_options(${PROJECT_NAME} PRIVATE /W3 /w34456 /w34189 /w44305 /w44244 /w44267)

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

# cpp/windows stuff
target_compile_definitions(${PROJECT_NAME} PRIVATE "UNICODE;_UNICODE")
set_property(TARGET DirectEngine PROPERTY CXX_STANDARD 20)

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:RELWITHDEBINFO>:/PROFILE>)
endif()

# tests
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_subdirectory(tests)
endif()

# custom flags
target_compile_definitions(${PROJECT_NAME} PRIVATE "ENABLE_TIMERS")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # target_compile_definitions(${PROJECT_NAME} PRIVATE "ENABLE_SHADER_HOTLOAD")
endif()