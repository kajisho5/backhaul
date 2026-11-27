# Everything that touches the Zoom Meeting SDK lives in this folder.
# The SDK is not part of this repository: point BACKHAUL_ZOOM_SDK_DIR at the "x64"
# folder of the unpacked Zoom Meeting SDK for Windows (it contains "h", "lib" and "bin").
if(NOT WIN32)
  message(FATAL_ERROR "The Zoom Meeting SDK part builds on Windows only.")
endif()
if(NOT EXISTS "${BACKHAUL_ZOOM_SDK_DIR}/h/zoom_sdk.h" OR NOT EXISTS "${BACKHAUL_ZOOM_SDK_DIR}/lib/sdk.lib")
  message(FATAL_ERROR "BACKHAUL_ZOOM_SDK_DIR must contain h/zoom_sdk.h and lib/sdk.lib (got '${BACKHAUL_ZOOM_SDK_DIR}').")
endif()

add_library(zoom_sdk SHARED IMPORTED)
set_target_properties(zoom_sdk PROPERTIES
  IMPORTED_IMPLIB "${BACKHAUL_ZOOM_SDK_DIR}/lib/sdk.lib"
  IMPORTED_LOCATION "${BACKHAUL_ZOOM_SDK_DIR}/bin/sdk.dll")
# SYSTEM: the SDK's own headers are not held to our warning level.
target_include_directories(zoom_sdk SYSTEM INTERFACE "${BACKHAUL_ZOOM_SDK_DIR}/h")

add_executable(backhaul_probe_receive probe/receive_probe.cpp)
target_link_libraries(backhaul_probe_receive PRIVATE backhaul_core zoom_sdk)
target_compile_definitions(backhaul_probe_receive PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
backhaul_warnings(backhaul_probe_receive strict)
