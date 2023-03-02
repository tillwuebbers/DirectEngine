# Main CMakeLists.txt
cmake_minimum_required (VERSION 3.12)

# dirs
set(XAUDIO2_DIR ${CMAKE_CURRENT_SOURCE_DIR}/xaudio2)
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(DX_HEADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/directx/headers)
set(GLM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glm)

set(GAME_NAME game)
set(TEST_PROJECT_NAME "DirectEngineTest")

function(DEPS PROJ)
    # imgui
    target_sources(${PROJ}
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

    target_include_directories(${PROJ}
                                PRIVATE ${IMGUI_DIR}
                                PRIVATE ${IMGUI_DIR}/backends
                                PRIVATE ${GLM_DIR})

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

    
    # edit and continue mode
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /ZI")
      set(CMAKE_SHARED_LINKER_FLAGS "/SAFESEH:NO")
      set(CMAKE_EXE_LINKER_FLAGS "/SAFESEH:NO")
    endif()
    
    # options
    if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_link_options(${PROJ} PRIVATE $<$<CONFIG:RELWITHDEBINFO>:/PROFILE>)
    endif()
    target_compile_definitions(${PROJ} PRIVATE "UNICODE;_UNICODE")
    set_property(TARGET ${PROJ} PROPERTY CXX_STANDARD 20)
endfunction()

# game
set(ENGINE_NAME "Engine")
add_executable(${ENGINE_NAME} WIN32 "DirectEngine.cpp" "DirectEngine.h")
target_link_libraries(${ENGINE_NAME} ${PROJECT_NAME})
DEPS(${ENGINE_NAME})

# assets
file(COPY "Montserrat-Regular.ttf" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY "remixicon.ttf" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
cmake_path(NATIVE_PATH SHADER_DIR SHADER_DIR_NATIVE)
set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders_bin")

add_custom_command(
        TARGET ${ENGINE_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/audio
                ${CMAKE_CURRENT_BINARY_DIR}/audio)

add_custom_command(
        TARGET ${ENGINE_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${SHADER_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders)

add_custom_command(
        TARGET ${ENGINE_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/models
                ${CMAKE_CURRENT_BINARY_DIR}/models)
                
add_custom_command(
        TARGET ${ENGINE_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/textures
                ${CMAKE_CURRENT_BINARY_DIR}/textures)
                
# compile shaders
target_include_directories(${ENGINE_NAME} PRIVATE "${SHADER_DIR}")
foreach(SHADER_FILE ${SHADER_CODE})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)

    add_custom_command(
        OUTPUT "${SHADER_OUT_DIR}/${SHADER_NAME}.vert.cso" "${SHADER_OUT_DIR}/${SHADER_NAME}.frag.cso"
        COMMAND dxc /T vs_6_0 /E "VSMain" /Fo "${SHADER_OUT_DIR}/${SHADER_NAME}.vert.cso" /I ${SHADER_DIR_NATIVE} "${SHADER_FILE}" && dxc /T ps_6_0 /E "PSMain" /Fo "${SHADER_OUT_DIR}/${SHADER_NAME}.frag.cso" /I ${SHADER_DIR_NATIVE} "${SHADER_FILE}"
        MAIN_DEPENDENCY "${SHADER_FILE}"
    )
endforeach()

# sub projects
add_subdirectory(core)
add_subdirectory(game)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_subdirectory(tests)
endif()
