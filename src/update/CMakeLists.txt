# Updating the program itself. Windows: WinSparkle, when its unpacked release folder is
# given to the build (BACKHAUL_WINSPARKLE_DIR, the folder with include/ and x64/Release/).
# Everywhere else, and without that folder, the program is built without an updater.
set(BACKHAUL_UPDATE_FEED_URL "https://github.com/kajisho5/backhaul-sdk/releases/download/updates/appcast.xml"
    CACHE STRING "The list of versions the updater reads")
# The public half of the key that signs installers (see packaging/README.md). Not a secret.
file(STRINGS "${CMAKE_SOURCE_DIR}/packaging/update_public_key.txt" BACKHAUL_UPDATE_PUBLIC_KEY LIMIT_COUNT 1)

if(WIN32 AND BACKHAUL_WINSPARKLE_DIR)
  if(NOT EXISTS "${BACKHAUL_WINSPARKLE_DIR}/include/winsparkle.h" OR NOT EXISTS "${BACKHAUL_WINSPARKLE_DIR}/x64/Release/WinSparkle.lib")
    message(FATAL_ERROR "BACKHAUL_WINSPARKLE_DIR must be the unpacked WinSparkle release folder (got '${BACKHAUL_WINSPARKLE_DIR}').")
  endif()
  if(NOT BACKHAUL_UPDATE_PUBLIC_KEY)
    message(FATAL_ERROR "packaging/update_public_key.txt is empty: an updater without a key would accept nothing.")
  endif()
  add_library(backhaul_update STATIC winsparkle_updater.cpp updater.h)
  target_include_directories(backhaul_update PRIVATE "${BACKHAUL_WINSPARKLE_DIR}/include")
  target_link_libraries(backhaul_update PRIVATE "${BACKHAUL_WINSPARKLE_DIR}/x64/Release/WinSparkle.lib")
  target_compile_definitions(backhaul_update PRIVATE
    BACKHAUL_UPDATE_FEED_URL="${BACKHAUL_UPDATE_FEED_URL}"
    BACKHAUL_UPDATE_PUBLIC_KEY="${BACKHAUL_UPDATE_PUBLIC_KEY}")
  set(BACKHAUL_HAS_UPDATER ON PARENT_SCOPE)
else()
  add_library(backhaul_update STATIC updater_none.cpp updater.h)
endif()
target_include_directories(backhaul_update PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
backhaul_warnings(backhaul_update strict)
