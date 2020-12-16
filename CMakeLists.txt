cmake_minimum_required(VERSION 3.0.0)
project(chatter VERSION 0.1.0)

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -fsanitize=address -pthread")

include(CTest)
enable_testing()

include_directories(include)
link_directories(lib)

file(GLOB SOURCES src/*.c)

add_executable(client src/client.c)
add_executable(client2 src/client2.c)
add_executable(server src/server.c)
target_link_libraries(client ${CURSES_LIBRARIES})
target_link_libraries(client2 ${CURSES_LIBRARIES})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
