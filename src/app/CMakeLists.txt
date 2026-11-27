find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Qml Quick)
qt_standard_project_setup()

# State and models, separate from the executable so they can be tested.
add_library(backhaul_app_state STATIC
  app_state.cpp app_state.h
  audio_controller.cpp audio_controller.h
  credential_store.cpp credential_store.h
  frame_store.cpp frame_store.h
  meeting_controller.cpp meeting_controller.h
  demo_source.cpp demo_source.h
  display_model.cpp display_model.h
  participant_model.cpp participant_model.h
  settings_saver.cpp settings_saver.h
  slot_model.cpp slot_model.h
)
target_include_directories(backhaul_app_state PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(backhaul_app_state PUBLIC backhaul_core backhaul_audio Qt6::Gui)
target_include_directories(backhaul_app_state PUBLIC ${CMAKE_SOURCE_DIR}/src)
if(WIN32)
  target_link_libraries(backhaul_app_state PRIVATE advapi32)
endif()
backhaul_warnings(backhaul_app_state app)

qt_add_executable(backhaul main.cpp frame_item.cpp frame_item.h)
if(TARGET backhaul_zoom)
  target_link_libraries(backhaul PRIVATE backhaul_zoom)
else()
  target_sources(backhaul PRIVATE meeting_none.cpp)
endif()

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(backhaul
  URI Backhaul
  VERSION 1.0
  RESOURCE_PREFIX /qt/qml
  QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/TopBar.qml
    qml/SlotTile.qml
    qml/ReturnTile.qml
    qml/RouteStrip.qml
    qml/DetailPanel.qml
    qml/ParticipantList.qml
    qml/Segmented.qml
    qml/Stepper.qml
    qml/PlainButton.qml
    qml/SettingsScreen.qml
    qml/SettingsAudio.qml
    qml/SettingsSlots.qml
    qml/LabeledInput.qml
    qml/SettingsVideo.qml
    qml/SettingsZoom.qml
    qml/OutputView.qml
    qml/Dropdown.qml
    qml/RoleSelect.qml
    qml/RoleScreen.qml
    qml/RoleTile.qml
    qml/TwoShotView.qml
)

target_link_libraries(backhaul PRIVATE backhaul_app_state Qt6::Qml Qt6::Quick)

# Window icon (all platforms) and the icon of the .exe file (Windows).
qt_add_resources(backhaul "appicon" PREFIX "/" BASE "${CMAKE_SOURCE_DIR}/assets/logo"
  FILES "${CMAKE_SOURCE_DIR}/assets/logo/icon-256.png")
if(WIN32)
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/backhaul.rc"
    "IDI_ICON1 ICON \"${CMAKE_SOURCE_DIR}/assets/logo/backhaul.ico\"\n")
  target_sources(backhaul PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/backhaul.rc")
endif()
backhaul_warnings(backhaul app)
if(MSVC)
  # The C++ that Qt generates from the QML files makes MSVC report "unreachable code"
  # (C4702) inside Qt's own headers. Only this target holds generated code; the
  # hand-written logic in backhaul_app_state keeps the full warning set.
  target_compile_options(backhaul PRIVATE /wd4702)
endif()

if(WIN32)
  set_target_properties(backhaul PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(BUILD_TESTING)
  add_executable(app_state_tests ${CMAKE_SOURCE_DIR}/tests/app/test_app_state.cpp)
  target_include_directories(app_state_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
  target_link_libraries(app_state_tests PRIVATE backhaul_app_state)
  backhaul_warnings(app_state_tests app)
  add_test(NAME app_state_tests COMMAND app_state_tests)
  set_tests_properties(app_state_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  add_executable(meeting_controller_tests ${CMAKE_SOURCE_DIR}/tests/app/test_meeting_controller.cpp)
  target_include_directories(meeting_controller_tests PRIVATE ${CMAKE_SOURCE_DIR}/tests)
  target_link_libraries(meeting_controller_tests PRIVATE backhaul_app_state)
  backhaul_warnings(meeting_controller_tests app)
  add_test(NAME meeting_controller_tests COMMAND meeting_controller_tests)
  set_tests_properties(meeting_controller_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
