cmake_minimum_required(VERSION 3.10.3)
project(luabug_nccalls C)
include(ExternalProject)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)
file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX})

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

foreach(FLAG
  -pipe
  -funwind-tables
  -funsigned-char
  -Werror
  -Wall
  -Wextra
  -Wno-strict-aliasing
  -Wno-unused-parameter
  -D_GNU_SOURCE
  -D_ISOC99_SOURCE
  -D_FILE_OFFSET_BITS=64
  -D_LARGEFILE_SOURCE
  -D_LARGE_FILES
  -DLUA_USE_LINUX
)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAG}")
endforeach()

include(lua.cmake)

add_executable(luabug_nccalls main.c)
add_dependencies(luabug_nccalls lua)
target_include_directories(luabug_nccalls PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_INSTALL_PREFIX}/include
)
target_link_libraries(luabug_nccalls
  ${CMAKE_INSTALL_PREFIX}/lib/liblua.a
  -lm ${CMAKE_DL_LIBS}
)
