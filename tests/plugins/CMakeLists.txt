# A real plug-in file, built from the SDK's plug-in side, so the host is tested by
# actually loading and running one (on Windows and on Linux).
set(VST3 ${BACKHAUL_VST3_DIR})
add_library(backhaul_test_plugin MODULE
  test_plugin.cpp
  ${VST3}/public.sdk/source/main/moduleinit.cpp
  ${VST3}/public.sdk/source/main/pluginfactory.cpp
  ${VST3}/public.sdk/source/common/pluginview.cpp
  ${VST3}/public.sdk/source/vst/vstaudioeffect.cpp
  ${VST3}/public.sdk/source/vst/vstbus.cpp
  ${VST3}/public.sdk/source/vst/vstcomponent.cpp
  ${VST3}/public.sdk/source/vst/vstcomponentbase.cpp
  ${VST3}/public.sdk/source/vst/vsteditcontroller.cpp
  ${VST3}/public.sdk/source/vst/vstparameters.cpp
)
target_link_libraries(backhaul_test_plugin PRIVATE backhaul_vst3_base)
if(WIN32)
  target_sources(backhaul_test_plugin PRIVATE ${VST3}/public.sdk/source/main/dllmain.cpp)
  set(plugin_arch x86_64-win)
  set(plugin_file SampleGain.vst3)
else()
  target_sources(backhaul_test_plugin PRIVATE ${VST3}/public.sdk/source/main/linuxmain.cpp)
  set(plugin_arch ${CMAKE_SYSTEM_PROCESSOR}-linux)
  set(plugin_file SampleGain.so)
endif()
if(MSVC)
  target_compile_options(backhaul_test_plugin PRIVATE /W0 /utf-8)
else()
  target_compile_options(backhaul_test_plugin PRIVATE -w)
endif()
# The folder layout VST 3 asks for: SampleGain.vst3/Contents/<system>/<file>
set(plugin_bundle ${CMAKE_CURRENT_BINARY_DIR}/SampleGain.vst3)
set_target_properties(backhaul_test_plugin PROPERTIES PREFIX "" OUTPUT_NAME SampleGain)
add_custom_command(TARGET backhaul_test_plugin POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory ${plugin_bundle}/Contents/${plugin_arch}
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:backhaul_test_plugin> ${plugin_bundle}/Contents/${plugin_arch}/${plugin_file})

add_executable(plugin_tests test_plugin_host.cpp ../check.h)
target_include_directories(plugin_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(plugin_tests PRIVATE backhaul_plugins)
add_dependencies(plugin_tests backhaul_test_plugin)
backhaul_warnings(plugin_tests strict)
add_test(NAME plugin_tests COMMAND plugin_tests ${plugin_bundle})
