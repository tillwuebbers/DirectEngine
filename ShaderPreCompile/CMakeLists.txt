project("ShaderPreCompile")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
add_executable(${PROJECT_NAME} "ShaderPreCompile.cpp")
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../DirectEngine/directx/headers)
target_link_libraries(${PROJECT_NAME} ${CORE_NAME})
target_compile_definitions(${PROJECT_NAME} PRIVATE "UNICODE;_UNICODE")
target_compile_options(${PROJECT_NAME} PUBLIC -std:c++20 /Zc:strictStrings- /wd4244 /wd4267 /wd4305)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../Tools/dxc/dxc.exe
			${CMAKE_CURRENT_BINARY_DIR}/dxc.exe
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../Tools/dxc/dxcompiler.dll
			${CMAKE_CURRENT_BINARY_DIR}/dxcompiler.dll
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
			${CMAKE_CURRENT_SOURCE_DIR}/../Tools/dxc/dxil.dll
			${CMAKE_CURRENT_BINARY_DIR}/dxil.dll)