cmake_minimum_required (VERSION 3.12)

project ("DirectEngineTest")
enable_testing()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable("DirectEngineTest" "Animation.cpp" "TestCommon.h" ${CORE_CODE} ${GAME_CODE} ${IMPORT_CODE})

target_sources("DirectEngineTest"
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

target_include_directories("DirectEngineTest" PRIVATE ../directx/headers)
target_include_directories("DirectEngineTest" PUBLIC ${XAUDIO2_DIR}/include)
target_include_directories("DirectEngineTest" PUBLIC ${IMGUI_DIR} PUBLIC ${IMGUI_DIR}/backends PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../glm )

file(COPY "${XAUDIO2_DIR}/debug/bin/x64/xaudio2_9redist.dll" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries("DirectEngineTest" "${XAUDIO2_DIR}/debug/lib/x64/xaudio2_9redist.lib")

target_link_libraries("DirectEngineTest" "d3d12.lib" "dxgi.lib" "dxguid.lib" "d3dcompiler.lib" GTest::gtest_main)
target_compile_options("DirectEngineTest" PRIVATE /W3 /w34456 /w34189 /w44305 /w44244 /w44267)

include(GoogleTest)
gtest_discover_tests("DirectEngineTest")

set_property(TARGET "DirectEngineTest" PROPERTY CXX_STANDARD 20)
target_compile_definitions("DirectEngineTest" PRIVATE "UNICODE;_UNICODE")
