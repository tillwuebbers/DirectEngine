# Top Level CMakeLists
cmake_minimum_required (VERSION 3.12)

set(CORE_NAME "Core")
set(GAME_NAME "Game")
set(TEST_PROJECT_NAME "DirectEngineTest")

project("DirectEngine")
enable_testing()
add_subdirectory("DirectEngine")
