// Used when the program is built without the Steinberg ASIO SDK.
#include "audio/audio_output.h"

namespace backhaul {
std::unique_ptr<AudioOutput> makeAsioOutput() { return nullptr; }
}  // namespace backhaul
