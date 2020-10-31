cmake_minimum_required(VERSION 3.0.0)
project(chatter VERSION 0.1.0)

find_package(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

add_definitions(${GTK3_CFLAGS_OTHER})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread -rdynamic")

include(CTest)
enable_testing()

include_directories(include)
link_directories(lib)

file(GLOB SOURCES src/*.c)

add_executable(chatter ${SOURCES})

target_link_libraries(chatter ${GTK3_LIBRARIES})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
