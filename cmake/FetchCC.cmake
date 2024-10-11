message(STATUS "[fc] cc fetching ...")

set(CC_ENABLE_COROUTINE ON)
set(CC_ENABLE_IO_URING OFF)
set(CC_ENABLE_ASAN OFF)
set(CC_WITH_YYJSON ON)
set(CC_WITH_SQLITE3 ON)

FetchContent_Declare(
  cc
  URL https://github.com/cc-hng/cc/archive/master.tar.gz
  DOWNLOAD_NO_PROGRESS TRUE)

list(APPEND MY_LIBS cc)
