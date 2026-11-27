// A sound device that plays what the mixer produces. The Windows implementation
// (WASAPI) lives in this folder; nothing else sees Windows audio types.
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace backhaul {

class AudioMixer;

struct AudioDevice {
  std::string id;    // stable handle for start()
  std::string name;  // what the operator reads (UTF-8)
};

class AudioOutput {
 public:
  virtual ~AudioOutput() = default;
  virtual std::vector<AudioDevice> devices() = 0;
  // Stops whatever was playing first. `mixer` must outlive the output.
  // On failure returns false and puts a short reason into `error`.
  virtual bool start(const std::string& deviceId, AudioMixer* mixer, std::string* error) = 0;
  virtual void stop() = 0;
  // False after stop(), and also when the device went away by itself.
  virtual bool running() const = 0;
  virtual int channels() const = 0;  // of the running device, 0 when stopped
};

// The system's own sound output (WASAPI on Windows). Null where there is none.
std::unique_ptr<AudioOutput> makeAudioOutput();
// Steinberg ASIO drivers. Null when the program was built without the ASIO SDK.
std::unique_ptr<AudioOutput> makeAsioOutput();

}  // namespace backhaul
