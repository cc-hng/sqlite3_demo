include(cmake/ProjectGuard.cmake)
cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

project(
  demo
  VERSION "0.1.0"
  LANGUAGES CXX C)

#####################################
# compile & link options
#####################################
include(cmake/common.cmake)

#####################################
# third-party packages
#####################################
set(MY_LIBS)
include(FetchContent)

include(cmake/FetchSqlite3.cmake)
include(cmake/FetchFmt.cmake)

FetchContent_MakeAvailable(${MY_LIBS})

#####################################
# target
#####################################
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/**.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${headers} ${sources})
target_link_libraries(${PROJECT_NAME} PRIVATE sqlite3 fmt::fmt)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(${PROJECT_NAME} PRIVATE -Wno-psabi)
endif()
