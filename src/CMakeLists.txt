cmake_minimum_required(VERSION 2.6)
project (libipho-screen-server)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

add_library(err-util STATIC err_util.c)
add_library(file-util STATIC file_util.c)
add_library(net-util STATIC net_util.c)
add_library(time-util STATIC time_util.c)

add_executable(libipho-screen-server libipho-screen-server.c)

target_link_libraries(libipho-screen-server
    pthread
    err-util
    file-util
    time-util
    net-util)

install(TARGETS libipho-screen-server
  RUNTIME DESTINATION bin
)

