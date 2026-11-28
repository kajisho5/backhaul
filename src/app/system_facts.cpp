#include "system_facts.h"

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>

SystemFacts readSystemFacts() {
  SystemFacts facts;
  SYSTEM_POWER_STATUS power{};
  if (GetSystemPowerStatus(&power) != FALSE && (power.ACLineStatus == 0 || power.ACLineStatus == 1)) {
    facts.power = power.ACLineStatus;
  }
  QUERY_USER_NOTIFICATION_STATE state{};
  if (SUCCEEDED(SHQueryUserNotificationState(&state))) {
    switch (state) {
      case QUNS_ACCEPTS_NOTIFICATIONS:
        facts.notifications = 1;
        break;
      case QUNS_BUSY:
      case QUNS_RUNNING_D3D_FULL_SCREEN:
      case QUNS_PRESENTATION_MODE:
      case QUNS_QUIET_TIME:
      case QUNS_APP:
        facts.notifications = 0;
        break;
      default:
        break;  // nobody at the PC, or an answer this program does not know
    }
  }
  return facts;
}

bool canKeepAwake() { return true; }

void keepAwake(bool on) {
  SetThreadExecutionState(on ? (ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED) : ES_CONTINUOUS);
}

#else

SystemFacts readSystemFacts() { return {}; }
bool canKeepAwake() { return false; }
void keepAwake(bool) {}

#endif
