#include "audio/audio_output.h"

namespace backhaul {
std::unique_ptr<AudioOutput> makeAudioOutput() { return nullptr; }
std::unique_ptr<AudioInput> makeAudioInput() { return nullptr; }

}  // namespace backhaul
