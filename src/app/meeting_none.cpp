// Used when the program is built without the Zoom Meeting SDK.
#include "meeting/meeting_client.h"

namespace backhaul {
std::unique_ptr<MeetingClient> makeMeetingClient() { return nullptr; }
}  // namespace backhaul
