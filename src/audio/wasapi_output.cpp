// AudioOutput on WASAPI (shared mode, event driven). Windows converts from our
// 48 kHz float format to whatever the device runs at.
#include <windows.h>

#include <objbase.h>

#include <initguid.h>

#include <mmreg.h>

#include <ks.h>

#include <ksmedia.h>

#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>

#include <atomic>
#include <cstdio>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "audio/audio_output.h"
#include "core/audio_mixer.h"

namespace backhaul {
namespace {

template <typename T>
class Com {
 public:
  Com() = default;
  ~Com() { reset(); }
  Com(const Com&) = delete;
  Com& operator=(const Com&) = delete;
  T** put() {
    reset();
    return &p_;
  }
  T* get() const { return p_; }
  T* operator->() const { return p_; }
  explicit operator bool() const { return p_ != nullptr; }
  void reset() {
    if (p_ != nullptr) {
      p_->Release();
      p_ = nullptr;
    }
  }

 private:
  T* p_ = nullptr;
};

class ComApartment {
 public:
  ComApartment() : ok_(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {}
  ~ComApartment() {
    if (ok_) CoUninitialize();
  }
  ComApartment(const ComApartment&) = delete;
  ComApartment& operator=(const ComApartment&) = delete;

 private:
  bool ok_;
};

std::string toUtf8(const wchar_t* text) {
  if (text == nullptr || *text == L'\0') return {};
  const int need = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (need <= 1) return {};
  std::string out(static_cast<std::size_t>(need), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), need, nullptr, nullptr);
  out.resize(static_cast<std::size_t>(need) - 1);
  return out;
}

std::wstring toWide(const std::string& text) {
  if (text.empty()) return {};
  const int need = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  if (need <= 1) return {};
  std::wstring out(static_cast<std::size_t>(need), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), need);
  out.resize(static_cast<std::size_t>(need) - 1);
  return out;
}

std::string hr(const char* what, HRESULT code) {
  char buffer[160];
  std::snprintf(buffer, sizeof(buffer), "%s (0x%08lX)", what, static_cast<unsigned long>(code));
  return buffer;
}

class WasapiOutput final : public AudioOutput {
 public:
  ~WasapiOutput() override { stop(); }

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
      if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.put()))) return;
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

  bool start(const std::string& deviceId, AudioMixer* mixer, std::string* error) override {
    stop();
    if (mixer == nullptr || deviceId.empty()) {
      if (error != nullptr) *error = "no device";
      return false;
    }
    stopRequested_ = false;
    std::promise<std::string> ready;  // empty string = started
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
    if (thread_.joinable()) thread_.join();
    running_ = false;
    channels_ = 0;
  }

  bool running() const override { return running_; }
  int channels() const override { return channels_; }

 private:
  void run(const std::string& deviceId, AudioMixer* mixer, std::promise<std::string>& ready) {
    const ComApartment apartment;
    Com<IMMDeviceEnumerator> enumerator;
    Com<IMMDevice> device;
    Com<IAudioClient> client;
    Com<IAudioRenderClient> render;
    HANDLE wake = nullptr;
    UINT32 bufferFrames = 0;
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
      format.Format.nSamplesPerSec = static_cast<DWORD>(AudioMixer::kRate);
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
      r = client->GetBufferSize(&bufferFrames);
      if (FAILED(r)) return hr("device buffer", r);
      r = client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(render.put()));
      if (FAILED(r)) return hr("device render", r);
      r = client->Start();
      if (FAILED(r)) return hr("device play", r);
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

    while (!stopRequested_) {
      if (WaitForSingleObject(wake, 200) != WAIT_OBJECT_0) continue;
      UINT32 padding = 0;
      if (FAILED(client->GetCurrentPadding(&padding))) break;  // device went away
      const UINT32 frames = bufferFrames - padding;
      if (frames == 0) continue;
      BYTE* data = nullptr;
      if (FAILED(render->GetBuffer(frames, &data)) || data == nullptr) break;
      mixer->render(reinterpret_cast<float*>(data), frames, channels);
      if (FAILED(render->ReleaseBuffer(frames, 0))) break;
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

std::unique_ptr<AudioOutput> makeAudioOutput() { return std::make_unique<WasapiOutput>(); }

}  // namespace backhaul
