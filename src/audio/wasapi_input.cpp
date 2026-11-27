// AudioInput on WASAPI (shared mode, event driven): a recording device of this PC,
// delivered as 48 kHz float. Windows converts from whatever the device runs at.
#include "audio/wasapi_common.h"

#include <atomic>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "audio/audio_output.h"

namespace backhaul {
namespace {

using namespace wasapi;

constexpr int kRate = 48000;

class WasapiInput final : public AudioInput {
 public:
  ~WasapiInput() override { stop(); }

  std::vector<AudioDevice> devices() override {
    std::vector<AudioDevice> list;
    // On its own thread so the caller's COM setup, whatever it is, does not matter.
    std::thread([&list] {
      const ComApartment apartment;
      Com<IMMDeviceEnumerator> enumerator;
      if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(enumerator.put())))) {
        return;
      }
      Com<IMMDeviceCollection> collection;
      if (FAILED(enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, collection.put()))) return;
      UINT count = 0;
      if (FAILED(collection->GetCount(&count))) return;
      for (UINT i = 0; i < count; ++i) {
        Com<IMMDevice> device;
        if (FAILED(collection->Item(i, device.put()))) continue;
        LPWSTR id = nullptr;
        if (FAILED(device->GetId(&id)) || id == nullptr) continue;
        AudioDevice entry;
        entry.id = toUtf8(id);
        CoTaskMemFree(id);
        Com<IPropertyStore> store;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, store.put()))) {
          PROPVARIANT value;
          PropVariantInit(&value);
          if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) {
            entry.name = toUtf8(value.pwszVal);
          }
          PropVariantClear(&value);
        }
        if (entry.name.empty()) entry.name = entry.id;
        list.push_back(std::move(entry));
      }
    }).join();
    return list;
  }

  bool start(const std::string& deviceId, AudioSink* sink, std::string* error) override {
    stop();
    if (sink == nullptr || deviceId.empty()) {
      if (error != nullptr) *error = "no device";
      return false;
    }
    stopRequested_ = false;
    std::promise<std::string> ready;  // empty string = started
    std::future<std::string> result = ready.get_future();
    thread_ = std::thread([this, deviceId, sink, &ready] { run(deviceId, sink, ready); });
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
    if (thread_.joinable()) thread_.join();
    running_ = false;
    channels_ = 0;
  }

  bool running() const override { return running_; }
  int channels() const override { return channels_; }

 private:
  void run(const std::string& deviceId, AudioSink* sink, std::promise<std::string>& ready) {
    const ComApartment apartment;
    Com<IMMDeviceEnumerator> enumerator;
    Com<IMMDevice> device;
    Com<IAudioClient> client;
    Com<IAudioCaptureClient> capture;
    HANDLE wake = nullptr;
    int channels = 0;

    const std::string problem = [&]() -> std::string {
      HRESULT r = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(enumerator.put()));
      if (FAILED(r)) return hr("device list", r);
      r = enumerator->GetDevice(toWide(deviceId).c_str(), device.put());
      if (FAILED(r)) return hr("device not found", r);
      r = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client.put()));
      if (FAILED(r)) return hr("device open", r);

      WAVEFORMATEX* mix = nullptr;
      r = client->GetMixFormat(&mix);
      if (FAILED(r) || mix == nullptr) return hr("device format", r);
      channels = mix->nChannels;
      DWORD channelMask = 0;
      if (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->cbSize >= 22) {
        channelMask = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix)->dwChannelMask;
      }
      CoTaskMemFree(mix);
      if (channels < 1 || channels > 64) return "unsupported channel count";

      WAVEFORMATEXTENSIBLE format{};
      format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
      format.Format.nChannels = static_cast<WORD>(channels);
      format.Format.nSamplesPerSec = static_cast<DWORD>(kRate);
      format.Format.wBitsPerSample = 32;
      format.Format.nBlockAlign = static_cast<WORD>(channels * 4);
      format.Format.nAvgBytesPerSec = format.Format.nSamplesPerSec * format.Format.nBlockAlign;
      format.Format.cbSize = 22;
      format.Samples.wValidBitsPerSample = 32;
      format.dwChannelMask = channelMask;
      format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

      const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                          AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
      r = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 200000 /* 20 ms */, 0, &format.Format, nullptr);
      if (FAILED(r)) return hr("device start", r);
      wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
      if (wake == nullptr) return "no event";
      r = client->SetEventHandle(wake);
      if (FAILED(r)) return hr("device event", r);
      r = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(capture.put()));
      if (FAILED(r)) return hr("device capture", r);
      r = client->Start();
      if (FAILED(r)) return hr("device record", r);
      return {};
    }();

    if (!problem.empty()) {
      if (wake != nullptr) CloseHandle(wake);
      ready.set_value(problem);
      return;
    }
    channels_ = channels;
    running_ = true;
    ready.set_value({});

    std::vector<float> silence;
    bool alive = true;
    while (alive && !stopRequested_) {
      if (WaitForSingleObject(wake, 200) != WAIT_OBJECT_0) continue;
      for (;;) {  // take every packet that is waiting
        UINT32 next = 0;
        if (FAILED(capture->GetNextPacketSize(&next))) {
          alive = false;  // device went away
          break;
        }
        if (next == 0) break;
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD packetFlags = 0;
        if (FAILED(capture->GetBuffer(&data, &frames, &packetFlags, nullptr, nullptr))) {
          alive = false;
          break;
        }
        if (frames > 0) {
          if ((packetFlags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr) {
            silence.assign(static_cast<std::size_t>(frames) * static_cast<std::size_t>(channels), 0.0f);
            sink->onCaptured(silence.data(), frames, channels);
          } else {
            sink->onCaptured(reinterpret_cast<const float*>(data), frames, channels);
          }
        }
        if (FAILED(capture->ReleaseBuffer(frames))) {
          alive = false;
          break;
        }
      }
    }
    client->Stop();
    CloseHandle(wake);
    running_ = false;
  }

  std::thread thread_;
  std::atomic<bool> stopRequested_{false};
  std::atomic<bool> running_{false};
  std::atomic<int> channels_{0};
};

}  // namespace

std::unique_ptr<AudioInput> makeAudioInput() { return std::make_unique<WasapiInput>(); }

}  // namespace backhaul
