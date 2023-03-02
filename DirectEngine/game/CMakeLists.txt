# Game
file (GLOB GAME_CODE *.h *.cpp)

add_library(${GAME_NAME} SHARED ${GAME_CODE})
target_link_libraries(${GAME_NAME} ${PROJECT_NAME})
DEPS(${GAME_NAME})