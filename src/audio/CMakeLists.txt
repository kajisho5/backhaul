# Sound devices. Windows: WASAPI, and Steinberg ASIO when its SDK is given to the build.
# Elsewhere there is no output (the mixer is still tested).
if(WIN32)
  add_library(backhaul_audio STATIC wasapi_output.cpp wasapi_input.cpp wasapi_common.h audio_output.h)
  target_link_libraries(backhaul_audio PRIVATE ole32)
  target_compile_definitions(backhaul_audio PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
else()
  add_library(backhaul_audio STATIC audio_output_none.cpp audio_output.h)
endif()
target_include_directories(backhaul_audio PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(backhaul_audio PUBLIC backhaul_core)
backhaul_warnings(backhaul_audio strict)

# ASIO. The SDK is not in this repository (see third_party/README.md): point
# BACKHAUL_ASIO_SDK_DIR at the unpacked "ASIOSDK" folder.
if(WIN32 AND BACKHAUL_ASIO_SDK_DIR)
  if(NOT EXISTS "${BACKHAUL_ASIO_SDK_DIR}/common/asio.h" OR NOT EXISTS "${BACKHAUL_ASIO_SDK_DIR}/host/asiodrivers.cpp")
    message(FATAL_ERROR "BACKHAUL_ASIO_SDK_DIR must be the ASIOSDK folder (got '${BACKHAUL_ASIO_SDK_DIR}').")
  endif()
  # Steinberg's host code, built as it comes (not held to our warning level).
  add_library(asio_sdk_host STATIC
    "${BACKHAUL_ASIO_SDK_DIR}/common/asio.cpp"
    "${BACKHAUL_ASIO_SDK_DIR}/host/asiodrivers.cpp"
    "${BACKHAUL_ASIO_SDK_DIR}/host/pc/asiolist.cpp")
  target_include_directories(asio_sdk_host SYSTEM PUBLIC
    "${BACKHAUL_ASIO_SDK_DIR}/common" "${BACKHAUL_ASIO_SDK_DIR}/host" "${BACKHAUL_ASIO_SDK_DIR}/host/pc")
  target_link_libraries(asio_sdk_host PUBLIC ole32)
  target_sources(backhaul_audio PRIVATE asio_output.cpp)
  target_link_libraries(backhaul_audio PRIVATE asio_sdk_host)
  target_compile_definitions(backhaul_audio PUBLIC BACKHAUL_HAVE_ASIO)
else()
  target_sources(backhaul_audio PRIVATE asio_none.cpp)
endif()
