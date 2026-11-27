// A device that delivers moving pictures (a USB capture box, a webcam). The Windows
// implementation (Media Foundation) lives in this folder; nothing else sees its types.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace backhaul {

struct VideoDevice {
  std::string id;    // stable handle for start()
  std::string name;  // what the operator reads (UTF-8)
};

// Takes pictures on the device's thread. NV12: `y` has `height` rows of `yStride`
// bytes, `uv` has (height + 1) / 2 rows of `uvStride` bytes. Valid during the call only.
class VideoSink {
 public:
  virtual ~VideoSink() = default;
  virtual void onFrame(const std::uint8_t* y, int yStride, const std::uint8_t* uv, int uvStride, int width,
                       int height, std::int64_t timeUs) = 0;
};

class VideoInput {
 public:
  virtual ~VideoInput() = default;
  virtual std::vector<VideoDevice> devices() = 0;
  // Stops whatever was running first. `sink` must outlive the input.
  // On failure returns false and puts a short reason into `error`.
  virtual bool start(const std::string& deviceId, VideoSink* sink, std::string* error) = 0;
  virtual void stop() = 0;
  // False after stop(), and also when the device went away by itself.
  virtual bool running() const = 0;
  // What the running device delivers; zeros when stopped. Rate in pictures per 1000 seconds
  // (59940 for 59.94).
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual int milliFps() const = 0;
};

// Capture devices of this PC (USB video class). Null where there are none.
std::unique_ptr<VideoInput> makeVideoInput();

}  // namespace backhaul
