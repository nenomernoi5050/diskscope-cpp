cmake_minimum_required(VERSION 3.20)

project(diskscope VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_FUZZERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  miniz
  URL https://github.com/richgel999/miniz/archive/refs/tags/3.0.2.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(miniz)

add_library(diskscope_core
  src/matcher.cpp
  src/ooxml.cpp
  src/report.cpp
  src/scanner.cpp
  src/text.cpp
)

target_include_directories(diskscope_core PUBLIC include)
target_link_libraries(diskscope_core PRIVATE miniz)

if(MSVC)
  target_compile_options(diskscope_core PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(diskscope_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(diskscope src/main.cpp)
target_link_libraries(diskscope PRIVATE diskscope_core)

enable_testing()
add_executable(diskscope_tests tests/test_main.cpp)
target_link_libraries(diskscope_tests PRIVATE diskscope_core)
add_test(NAME diskscope_tests COMMAND diskscope_tests)

