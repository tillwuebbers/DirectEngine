# Game
project("${GAME_NAME}")

add_library(${PROJECT_NAME} SHARED ${GAME_CODE})

target_link_libraries(${PROJECT_NAME} ${CORE_NAME})
DEPS(${PROJECT_NAME})