add_library(backhaul_core STATIC
  aspect_fit.cpp
  audio_mixer.cpp
  delay_line.cpp
  quality_policy.cpp
  resampler.cpp
  role.cpp
  route_matrix.cpp
  sdk_token.cpp
  sha256.cpp
  slot_output.cpp
  yuv.cpp
)
target_include_directories(backhaul_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
backhaul_warnings(backhaul_core strict)
