add_library(backhaul_core STATIC
  aspect_fit.cpp
  delay_line.cpp
  quality_policy.cpp
  resampler.cpp
  route_matrix.cpp
  slot_output.cpp
)
target_include_directories(backhaul_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
backhaul_warnings(backhaul_core strict)
