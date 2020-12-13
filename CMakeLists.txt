cmake_minimum_required(VERSION 3.0.0)
project(chatter VERSION 0.1.0)

find_package(PkgConfig REQUIRED)
PKG_CHECK_MODULES(GTK REQUIRED gtk+-3.0)

include_directories(${GTK_INCLUDE_DIRS})
link_directories(${GTK_LIBRARY_DIRS})

add_definitions(${GTK_CFLAGS_OTHER})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread -rdynamic")

# necessary files
# set (dep_files
#     src/window_main.glade
# )

# foreach(df ${dep_files})
#     file(COPY ${df} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})    
#     set (df_file_path ${CMAKE_CURRENT_BINARY_DIR}/${df})
# endforeach(df ${dep_files})

include(CTest)
enable_testing()

include_directories(include)
link_directories(lib)

file(GLOB SOURCES src/*.c)

add_executable(client src/client.c)
add_executable(server src/server.c)
# add_executable(client_console src/client_console.c)
target_link_libraries(client ${GTK_LIBRARIES})
target_link_libraries(server ${GTK_LIBRARIES})
# target_link_libraries(client_console ${GTK_LIBRARIES})

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
