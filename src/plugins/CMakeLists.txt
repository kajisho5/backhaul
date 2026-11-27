# Plug-in hosting. The VST 3 SDK (MIT licence since 3.8) is downloaded at configure time
# at a fixed version; -DFETCHCONTENT_SOURCE_DIR_VST3SDK=<folder> uses a local copy instead.
if(NOT BACKHAUL_PLUGINS)
  add_library(backhaul_plugins STATIC plugin_host_none.cpp plugin_host.h)
  target_link_libraries(backhaul_plugins PUBLIC backhaul_core)
  backhaul_warnings(backhaul_plugins strict)
  return()
endif()

include(FetchContent)
FetchContent_Declare(vst3sdk
  GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
  GIT_TAG v3.8.1_build_84
  GIT_SHALLOW TRUE
  GIT_SUBMODULES base pluginterfaces public.sdk
  GIT_SUBMODULES_RECURSE FALSE
  SOURCE_SUBDIR not-built-with-its-own-cmake  # only the sources below are used
)
FetchContent_MakeAvailable(vst3sdk)
set(VST3 ${vst3sdk_SOURCE_DIR})
set(BACKHAUL_VST3_DIR ${VST3} CACHE INTERNAL "where the VST 3 SDK sources are")

# The parts of the SDK a host needs. Steinberg's code, so without our warning set.
add_library(backhaul_vst3_base STATIC
  ${VST3}/base/source/baseiids.cpp
  ${VST3}/base/source/fbuffer.cpp
  ${VST3}/base/source/fdebug.cpp
  ${VST3}/base/source/fdynlib.cpp
  ${VST3}/base/source/fobject.cpp
  ${VST3}/base/source/fstreamer.cpp
  ${VST3}/base/source/fstring.cpp
  ${VST3}/base/source/timer.cpp
  ${VST3}/base/source/updatehandler.cpp
  ${VST3}/base/thread/source/fcondition.cpp
  ${VST3}/base/thread/source/flock.cpp
  ${VST3}/pluginterfaces/base/conststringtable.cpp
  ${VST3}/pluginterfaces/base/coreiids.cpp
  ${VST3}/pluginterfaces/base/funknown.cpp
  ${VST3}/pluginterfaces/base/ustring.cpp
  ${VST3}/public.sdk/source/common/commoniids.cpp
  ${VST3}/public.sdk/source/common/commonstringconvert.cpp
  ${VST3}/public.sdk/source/common/memorystream.cpp
  ${VST3}/public.sdk/source/vst/vstinitiids.cpp
  ${VST3}/public.sdk/source/vst/utility/stringconvert.cpp
)
target_include_directories(backhaul_vst3_base SYSTEM PUBLIC ${VST3})
target_compile_definitions(backhaul_vst3_base PUBLIC $<IF:$<CONFIG:Debug>,DEVELOPMENT=1,RELEASE=1>)
# The SDK is written for C++17 (its Windows loader does not compile as C++20).
set_target_properties(backhaul_vst3_base PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_STANDARD 17)
if(MSVC)
  target_compile_options(backhaul_vst3_base PRIVATE /W0 /utf-8)
  target_compile_definitions(backhaul_vst3_base PUBLIC UNICODE _UNICODE NOMINMAX)
else()
  target_compile_options(backhaul_vst3_base PRIVATE -w)
  find_package(Threads REQUIRED)
  target_link_libraries(backhaul_vst3_base PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
endif()

add_library(backhaul_vst3_hosting STATIC
  ${VST3}/public.sdk/source/vst/hosting/connectionproxy.cpp
  ${VST3}/public.sdk/source/vst/hosting/eventlist.cpp
  ${VST3}/public.sdk/source/vst/hosting/hostclasses.cpp
  ${VST3}/public.sdk/source/vst/hosting/module.cpp
  ${VST3}/public.sdk/source/vst/hosting/parameterchanges.cpp
  ${VST3}/public.sdk/source/vst/hosting/pluginterfacesupport.cpp
  ${VST3}/public.sdk/source/vst/hosting/plugprovider.cpp
)
if(WIN32)
  target_sources(backhaul_vst3_hosting PRIVATE
    ${VST3}/public.sdk/source/vst/hosting/module_win32.cpp
    ${VST3}/public.sdk/source/common/threadchecker_win32.cpp)
  target_link_libraries(backhaul_vst3_hosting PUBLIC ole32 shell32)
else()
  target_sources(backhaul_vst3_hosting PRIVATE
    ${VST3}/public.sdk/source/vst/hosting/module_linux.cpp
    ${VST3}/public.sdk/source/common/threadchecker_linux.cpp)
endif()
target_link_libraries(backhaul_vst3_hosting PUBLIC backhaul_vst3_base)
set_target_properties(backhaul_vst3_hosting PROPERTIES CXX_STANDARD 17)
if(MSVC)
  target_compile_options(backhaul_vst3_hosting PRIVATE /W0 /utf-8)
else()
  target_compile_options(backhaul_vst3_hosting PRIVATE -w)
endif()

add_library(backhaul_plugins STATIC vst3_host.cpp plugin_host.h)
target_link_libraries(backhaul_plugins PUBLIC backhaul_core PRIVATE backhaul_vst3_hosting)
if(WIN32)
  target_link_libraries(backhaul_plugins PRIVATE user32)
endif()
backhaul_warnings(backhaul_plugins app)
