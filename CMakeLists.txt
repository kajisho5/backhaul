cmake_minimum_required(VERSION 3.24)
project(backhaul VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /WX /utf-8 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Werror)
endif()

add_subdirectory(src/core)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
