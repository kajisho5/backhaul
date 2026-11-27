// AudioOutput on Steinberg ASIO. Built only when the ASIO SDK is given to the build
// (BACKHAUL_ASIO_SDK_DIR); the SDK itself is not part of this repository.
// The driver runs on a thread of its own with a message loop, as ASIO drivers expect.
#include <windows.h>

#include <objbase.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "asiosys.h"

#include "asio.h"

#include "asiodrivers.h"

#include "audio/audio_output.h"
#include "core/audio_mixer.h"

extern AsioDrivers* asioDrivers;
bool loadAsioDriver(char* name);

namespace backhaul {
namespace {

constexpr long kMaxDrivers = 64;
constexpr int kMaxOutputs = 64;

std::string asioError(const char* what, ASIOError code) {
  return std::string(what) + " (" + std::to_string(static_cast<long>(code)) + ")";
}

// ASIO has one driver and plain function callbacks, so the running output is found here.
class AsioOutput;
std::atomic<AsioOutput*> g_running{nullptr};

class AsioOutput final : public AudioOutput {
 public:
  ~AsioOutput() override { stop(); }

  std::vector<AudioDevice> devices() override {
    std::vector<AudioDevice> list;
    AsioDrivers drivers;
    std::vector<std::vector<char>> storage(static_cast<std::size_t>(kMaxDrivers), std::vector<char>(64, '\0'));
    std::vector<char*> names;
    for (auto& s : storage) names.push_back(s.data());
    const long count = drivers.getDriverNames(names.data(), kMaxDrivers);
    for (long i = 0; i < count && i < kMaxDrivers; ++i) {
      const std::string name = toUtf8(names[static_cast<std::size_t>(i)]);
      if (!name.empty()) list.push_back({names[static_cast<std::size_t>(i)], name});
    }
    return list;
  }

  bool start(const std::string& deviceId, AudioMixer* mixer, std::string* error) override {
    stop();
    if (mixer == nullptr || deviceId.empty() || deviceId.size() >= 64) {
      if (error != nullptr) *error = "no device";
      return false;
    }
    stopRequested_ = false;
    std::promise<std::string> ready;
    std::future<std::string> result = ready.get_future();
    thread_ = std::thread([this, deviceId, mixer, &ready] { run(deviceId, mixer, ready); });
    const std::string problem = result.get();
    if (!problem.empty()) {
      thread_.join();
      if (error != nullptr) *error = problem;
      return false;
    }
    return true;
  }

  void stop() override {
    stopRequested_ = true;
    if (thread_.joinable()) {
      PostThreadMessageW(threadId_, WM_NULL, 0, 0);  // wake the message loop
      thread_.join();
    }
    running_ = false;
    channels_ = 0;
  }

  bool running() const override { return running_; }
  int channels() const override { return channels_; }

 private:
  // Driver names are in the system's legacy code page.
  static std::string toUtf8(const char* ansi) {
    if (ansi == nullptr || *ansi == '\0') return {};
    const int wide = MultiByteToWideChar(CP_ACP, 0, ansi, -1, nullptr, 0);
    if (wide <= 1) return {};
    std::wstring w(static_cast<std::size_t>(wide), L'\0');
    MultiByteToWideChar(CP_ACP, 0, ansi, -1, w.data(), wide);
    const int need = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (need <= 1) return {};
    std::string out(static_cast<std::size_t>(need), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, out.data(), need, nullptr, nullptr);
    out.resize(static_cast<std::size_t>(need) - 1);
    return out;
  }

  static bool supported(ASIOSampleType type) {
    return type == ASIOSTInt16LSB || type == ASIOSTInt24LSB || type == ASIOSTInt32LSB || type == ASIOSTFloat32LSB ||
           type == ASIOSTFloat64LSB;
  }

  // One channel of the mixed block into the driver's buffer, in the driver's sample format.
  static void writeChannel(const float* interleaved, int channel, int channels, long frames, void* out,
                           ASIOSampleType type) {
    const float* in = interleaved + channel;
    switch (type) {
      case ASIOSTFloat32LSB: {
        float* dst = static_cast<float*>(out);
        for (long i = 0; i < frames; ++i) dst[i] = in[static_cast<std::ptrdiff_t>(i) * channels];
        break;
      }
      case ASIOSTFloat64LSB: {
        double* dst = static_cast<double*>(out);
        for (long i = 0; i < frames; ++i) dst[i] = static_cast<double>(in[static_cast<std::ptrdiff_t>(i) * channels]);
        break;
      }
      case ASIOSTInt32LSB: {
        std::int32_t* dst = static_cast<std::int32_t*>(out);
        for (long i = 0; i < frames; ++i) dst[i] = toInt(in[static_cast<std::ptrdiff_t>(i) * channels], 2147483647.0);
        break;
      }
      case ASIOSTInt24LSB: {
        std::uint8_t* dst = static_cast<std::uint8_t*>(out);
        for (long i = 0; i < frames; ++i) {
          const std::int32_t v = toInt(in[static_cast<std::ptrdiff_t>(i) * channels], 8388607.0);
          dst[i * 3] = static_cast<std::uint8_t>(v & 0xff);
          dst[i * 3 + 1] = static_cast<std::uint8_t>((v >> 8) & 0xff);
          dst[i * 3 + 2] = static_cast<std::uint8_t>((v >> 16) & 0xff);
        }
        break;
      }
      case ASIOSTInt16LSB: {
        std::int16_t* dst = static_cast<std::int16_t*>(out);
        for (long i = 0; i < frames; ++i) {
          dst[i] = static_cast<std::int16_t>(toInt(in[static_cast<std::ptrdiff_t>(i) * channels], 32767.0));
        }
        break;
      }
      default:
        break;
    }
  }

  static std::int32_t toInt(float sample, double fullScale) {
    double v = static_cast<double>(sample);
    if (v > 1.0) v = 1.0;
    if (v < -1.0) v = -1.0;
    return static_cast<std::int32_t>(v * fullScale);
  }

  void process(long index) {
    if (mixer_ == nullptr || index < 0 || index > 1) return;
    mixer_->render(block_.data(), static_cast<std::size_t>(bufferFrames_), outputs_);
    for (int c = 0; c < outputs_; ++c) {
      void* target = buffers_[static_cast<std::size_t>(c)].buffers[index];
      if (target != nullptr) writeChannel(block_.data(), c, outputs_, bufferFrames_, target, types_[static_cast<std::size_t>(c)]);
    }
    if (outputReady_) ASIOOutputReady();
  }

  static void onBufferSwitch(long index, ASIOBool) {
    AsioOutput* self = g_running.load(std::memory_order_acquire);
    if (self != nullptr) self->process(index);
  }
  static ASIOTime* onBufferSwitchTimeInfo(ASIOTime* time, long index, ASIOBool direct) {
    onBufferSwitch(index, direct);
    return time;
  }
  static void onSampleRateChanged(ASIOSampleRate) {}
  static long onMessage(long selector, long value, void*, double*) {
    switch (selector) {
      case kAsioSelectorSupported:
        return (value == kAsioEngineVersion || value == kAsioResetRequest || value == kAsioResyncRequest) ? 1 : 0;
      case kAsioEngineVersion:
        return 2;
      case kAsioResetRequest: {
        // The driver wants to be set up again (settings changed in its panel). Stop;
        // the application notices and asks the operator to choose the device again.
        AsioOutput* self = g_running.load(std::memory_order_acquire);
        if (self != nullptr) {
          self->stopRequested_ = true;
          PostThreadMessageW(self->threadId_, WM_NULL, 0, 0);
        }
        return 1;
      }
      case kAsioResyncRequest:
        return 1;
      default:
        return 0;
    }
  }

  void run(const std::string& deviceId, AudioMixer* mixer, std::promise<std::string>& ready) {
    threadId_ = GetCurrentThreadId();
    MSG msg{};
    PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);  // create the message queue
    const bool com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));

    bool loaded = false, initialised = false, haveBuffers = false, started = false;
    const std::string problem = [&]() -> std::string {
      char name[64] = {};
      std::memcpy(name, deviceId.c_str(), deviceId.size());
      if (!loadAsioDriver(name)) return "driver not found";
      loaded = true;
      ASIODriverInfo info{};
      info.asioVersion = 2;
      info.sysRef = nullptr;
      ASIOError r = ASIOInit(&info);
      if (r != ASE_OK) return std::string("driver did not start: ") + info.errorMessage;
      initialised = true;

      long inputs = 0, outputs = 0;
      r = ASIOGetChannels(&inputs, &outputs);
      if (r != ASE_OK || outputs < 1) return asioError("no output channels", r);
      outputs_ = static_cast<int>(outputs > kMaxOutputs ? kMaxOutputs : outputs);

      ASIOSampleRate rate = 0.0;
      if (ASIOGetSampleRate(&rate) != ASE_OK || rate != static_cast<double>(AudioMixer::kRate)) {
        r = ASIOSetSampleRate(static_cast<double>(AudioMixer::kRate));
        if (r != ASE_OK) return "the device is not set to 48 kHz and could not be switched";
      }

      long minSize = 0, maxSize = 0, preferred = 0, granularity = 0;
      r = ASIOGetBufferSize(&minSize, &maxSize, &preferred, &granularity);
      if (r != ASE_OK || preferred < 1 || preferred > 1 << 16) return asioError("buffer size", r);
      bufferFrames_ = preferred;

      buffers_.assign(static_cast<std::size_t>(outputs_), ASIOBufferInfo{});
      for (int c = 0; c < outputs_; ++c) {
        buffers_[static_cast<std::size_t>(c)].isInput = ASIOFalse;
        buffers_[static_cast<std::size_t>(c)].channelNum = c;
      }
      callbacks_.bufferSwitch = &AsioOutput::onBufferSwitch;
      callbacks_.sampleRateDidChange = &AsioOutput::onSampleRateChanged;
      callbacks_.asioMessage = &AsioOutput::onMessage;
      callbacks_.bufferSwitchTimeInfo = &AsioOutput::onBufferSwitchTimeInfo;
      r = ASIOCreateBuffers(buffers_.data(), outputs_, bufferFrames_, &callbacks_);
      if (r != ASE_OK) return asioError("buffers", r);
      haveBuffers = true;

      types_.assign(static_cast<std::size_t>(outputs_), ASIOSTInt32LSB);
      for (int c = 0; c < outputs_; ++c) {
        ASIOChannelInfo channel{};
        channel.channel = c;
        channel.isInput = ASIOFalse;
        r = ASIOGetChannelInfo(&channel);
        if (r != ASE_OK) return asioError("channel info", r);
        if (!supported(channel.type)) return "sample format " + std::to_string(static_cast<long>(channel.type)) + " is not supported";
        types_[static_cast<std::size_t>(c)] = channel.type;
      }
      block_.assign(static_cast<std::size_t>(bufferFrames_) * static_cast<std::size_t>(outputs_), 0.0f);
      outputReady_ = ASIOOutputReady() == ASE_OK;
      mixer_ = mixer;
      g_running.store(this, std::memory_order_release);
      r = ASIOStart();
      if (r != ASE_OK) return asioError("start", r);
      started = true;
      return {};
    }();

    if (problem.empty()) {
      channels_ = outputs_;
      running_ = true;
      ready.set_value({});
      while (!stopRequested_) {
        const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
        if (got <= 0) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
      }
    }

    if (started) ASIOStop();
    g_running.store(nullptr, std::memory_order_release);
    mixer_ = nullptr;
    if (haveBuffers) ASIODisposeBuffers();
    if (initialised) ASIOExit();
    if (loaded && asioDrivers != nullptr) asioDrivers->removeCurrentDriver();
    if (com) CoUninitialize();
    running_ = false;
    if (!problem.empty()) ready.set_value(problem);
  }

  std::thread thread_;
  DWORD threadId_ = 0;
  std::atomic<bool> stopRequested_{false};
  std::atomic<bool> running_{false};
  std::atomic<int> channels_{0};

  // Set up on the driver thread before the callbacks start; read by the callbacks.
  AudioMixer* mixer_ = nullptr;
  int outputs_ = 0;
  long bufferFrames_ = 0;
  bool outputReady_ = false;
  std::vector<ASIOBufferInfo> buffers_;
  std::vector<ASIOSampleType> types_;
  std::vector<float> block_;
  ASIOCallbacks callbacks_{};
};

}  // namespace

std::unique_ptr<AudioOutput> makeAsioOutput() { return std::make_unique<AsioOutput>(); }

}  // namespace backhaul
