cmake_minimum_required(VERSION 3.24)
project(backhaul VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BACKHAUL_BUILD_APP "Build the Qt operator application (needs Qt 6.4 or newer)" OFF)
set(BACKHAUL_ASIO_SDK_DIR "" CACHE PATH "The unpacked Steinberg ASIOSDK folder; empty builds without ASIO")
set(BACKHAUL_ZOOM_SDK_DIR "" CACHE PATH "x64 folder of the unpacked Zoom Meeting SDK for Windows; empty builds without Zoom")

# Our own code is built with warnings as errors. The strictest set is used for the
# core and its tests; the Qt application uses a slightly smaller set because Qt's
# generated code does not pass -Wconversion / -Wpedantic.
function(backhaul_warnings target level)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /permissive-)
  elseif(level STREQUAL "strict")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Werror)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
  endif()
endfunction()

include(CTest)

add_subdirectory(src/core)

# Before the app, which links the Zoom part when it exists.
if(BACKHAUL_ZOOM_SDK_DIR)
  add_subdirectory(src/zoom)
endif()

if(BACKHAUL_BUILD_APP)
  add_subdirectory(src/audio)
  add_subdirectory(src/video)
  add_subdirectory(src/app)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
