# Tests
project (${TEST_PROJECT_NAME})

cmake_policy(SET CMP0135 NEW)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(${PROJECT_NAME} "GameTest.cpp" "TestCommon.h" ${CORE_CODE} ${GAME_CODE} ${IMPORT_CODE} "EngineTest.cpp" "Tests.cpp")
DEPS(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
