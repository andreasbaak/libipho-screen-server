cmake_minimum_required(VERSION 2.6)
project (libipho-screen-server)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

add_executable(libipho-screen-server libipho-screen-server.c)

install(TARGETS libipho-screen-server
  RUNTIME DESTINATION bin
)

