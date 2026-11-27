// A sound device that plays what the mixer produces. The Windows implementation
// (WASAPI) lives in this folder; nothing else sees Windows audio types.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace backhaul {

class AudioMixer;

struct AudioDevice {
  std::string id;    // stable handle for start()
  std::string name;  // what the operator reads (UTF-8)
};

// Takes sound that a device recorded. Called on the device's thread:
// 48 kHz float samples, channel-interleaved.
class AudioSink {
 public:
  virtual ~AudioSink() = default;
  virtual void onCaptured(const float* interleaved, std::size_t frames, int channels) = 0;
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

  // Drivers that play and record on one device (ASIO): how many input channels the
  // running device has, and where their sound goes. All input channels are delivered
  // together. Null stops the delivery; clear it before the sink goes away.
  virtual int inputChannels() const { return 0; }
  virtual void setInputSink(AudioSink* sink) { (void)sink; }
};

// A recording device of its own (not tied to an output).
class AudioInput {
 public:
  virtual ~AudioInput() = default;
  virtual std::vector<AudioDevice> devices() = 0;
  // Stops whatever was recording first. `sink` must outlive the input.
  virtual bool start(const std::string& deviceId, AudioSink* sink, std::string* error) = 0;
  virtual void stop() = 0;
  virtual bool running() const = 0;
  virtual int channels() const = 0;  // of the running device, 0 when stopped
};

// The system's own sound output (WASAPI on Windows). Null where there is none.
std::unique_ptr<AudioOutput> makeAudioOutput();
// Steinberg ASIO drivers. Null when the program was built without the ASIO SDK.
std::unique_ptr<AudioOutput> makeAsioOutput();
// The system's recording devices (WASAPI on Windows). Null where there are none.
std::unique_ptr<AudioInput> makeAudioInput();

}  // namespace backhaul
