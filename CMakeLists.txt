# Top Level CMakeLists
cmake_minimum_required (VERSION 3.12)

set(CORE_NAME "Core")
set(GAME_NAME "Game")
set(TEST_PROJECT_NAME "DirectEngineTest")

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(CMAKE_CXX_FLAGS_DEBUG "/ZI")
	set(CMAKE_SHARED_LINKER_FLAGS "/SAFESEH:NO")
	set(CMAKE_EXE_LINKER_FLAGS "/SAFESEH:NO")
endif()

project("DirectEngine")
enable_testing()
add_subdirectory("DirectEngine")
