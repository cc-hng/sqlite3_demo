message(STATUS "[fc] lua fetching ...")

FetchContent_Declare(lua URL https://github.com/lua/lua/archive/refs/tags/v5.4.7.tar.gz)

FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
  FetchContent_Populate(lua)
  # lua has no CMake support, so we create our own target
  set(lua_sources ${lua_SOURCE_DIR}/onelua.c)
  add_library(lua STATIC ${lua_sources})
  set_target_properties(lua PROPERTIES LANGUAGE C)
  set_target_properties(lua PROPERTIES LINKER_LANGUAGE C)
  target_compile_options(lua PRIVATE -DMAKE_LIB)
  target_link_options(lua PRIVATE -Wno-deprecated-declarations)
  target_include_directories(lua PUBLIC $<BUILD_INTERFACE:${lua_SOURCE_DIR}>)
endif()
