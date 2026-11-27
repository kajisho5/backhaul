add_executable(core_tests
  check.h
  main.cpp
  test_aspect_fit.cpp
  test_audio_mixer.cpp
  test_delay_line.cpp
  test_mic_feed.cpp
  test_nv12.cpp
  test_quality_policy.cpp
  test_resampler.cpp
  test_role.cpp
  test_route_matrix.cpp
  test_sdk_token.cpp
  test_slot_output.cpp
  test_yuv.cpp
)
target_link_libraries(core_tests PRIVATE backhaul_core)
backhaul_warnings(core_tests strict)
add_test(NAME core_tests COMMAND core_tests)
