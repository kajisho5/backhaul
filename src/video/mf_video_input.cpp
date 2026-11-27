// VideoInput on Media Foundation: USB capture boxes and cameras that Windows lists as
// cameras. Windows decodes and converts whatever the device sends into NV12.
#include <windows.h>

#include <objbase.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>

#include <atomic>
#include <cstdio>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "video/video_input.h"

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

// COM and Media Foundation for one thread, for as long as this lives.
class MediaSession {
 public:
  MediaSession() : com_(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))), mf_(SUCCEEDED(MFStartup(MF_VERSION))) {}
  ~MediaSession() {
    if (mf_) MFShutdown();
    if (com_) CoUninitialize();
  }
  MediaSession(const MediaSession&) = delete;
  MediaSession& operator=(const MediaSession&) = delete;
  bool ok() const { return mf_; }

 private:
  bool com_;
  bool mf_;
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

std::string textOf(IMFActivate* device, REFGUID key) {
  WCHAR* value = nullptr;
  UINT32 length = 0;
  std::string out;
  if (SUCCEEDED(device->GetAllocatedString(key, &value, &length)) && value != nullptr) out = toUtf8(value);
  CoTaskMemFree(value);
  return out;
}

class MfVideoInput final : public VideoInput {
 public:
  ~MfVideoInput() override { stop(); }

  std::vector<VideoDevice> devices() override {
    std::vector<VideoDevice> list;
    // On its own thread so the caller's COM setup, whatever it is, does not matter.
    std::thread([&list] {
      const MediaSession session;
      if (!session.ok()) return;
      Com<IMFAttributes> wanted;
      if (FAILED(MFCreateAttributes(wanted.put(), 1))) return;
      if (FAILED(wanted->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID))) return;
      IMFActivate** found = nullptr;
      UINT32 count = 0;
      if (FAILED(MFEnumDeviceSources(wanted.get(), &found, &count)) || found == nullptr) return;
      for (UINT32 i = 0; i < count; ++i) {
        if (found[i] == nullptr) continue;
        VideoDevice entry;
        entry.id = textOf(found[i], MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);
        entry.name = textOf(found[i], MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);
        if (entry.name.empty()) entry.name = entry.id;
        if (!entry.id.empty()) list.push_back(std::move(entry));
        found[i]->Release();
      }
      CoTaskMemFree(found);
    }).join();
    return list;
  }

  bool start(const std::string& deviceId, VideoSink* sink, std::string* error) override {
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
    width_ = height_ = milliFps_ = 0;
  }

  bool running() const override { return running_; }
  int width() const override { return width_; }
  int height() const override { return height_; }
  int milliFps() const override { return milliFps_; }

 private:
  static constexpr DWORD kStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

  // The device's own mode to use: the largest picture up to 1920x1080, and among
  // those the fastest up to 60 per second.
  static bool pickNative(IMFSourceReader* reader, Com<IMFMediaType>& chosen, UINT32& width, UINT32& height,
                         UINT32& milliFps) {
    std::uint64_t best = 0;
    for (DWORD i = 0;; ++i) {
      Com<IMFMediaType> type;
      const HRESULT r = reader->GetNativeMediaType(kStream, i, type.put());
      if (r == MF_E_NO_MORE_TYPES || FAILED(r)) break;
      UINT32 w = 0, h = 0, num = 0, den = 0;
      if (FAILED(MFGetAttributeSize(type.get(), MF_MT_FRAME_SIZE, &w, &h)) || w == 0 || h == 0) continue;
      if (w > 1920 || h > 1080) continue;
      if (FAILED(MFGetAttributeRatio(type.get(), MF_MT_FRAME_RATE, &num, &den)) || den == 0) {
        num = 30;
        den = 1;
      }
      const std::uint64_t rate = static_cast<std::uint64_t>(num) * 1000 / den;
      if (rate > 61000) continue;
      const std::uint64_t score = static_cast<std::uint64_t>(w) * h * 100000 + rate + 1;
      if (score > best) {
        best = score;
        width = w;
        height = h;
        milliFps = static_cast<UINT32>(rate);
        chosen.reset();
        type->AddRef();
        *chosen.put() = type.get();
      }
    }
    return best != 0;
  }

  void run(const std::string& deviceId, VideoSink* sink, std::promise<std::string>& ready) {
    const MediaSession session;
    Com<IMFMediaSource> source;
    Com<IMFSourceReader> reader;
    UINT32 width = 0, height = 0, milliFps = 0;

    const std::string problem = [&]() -> std::string {
      if (!session.ok()) return "Media Foundation is not available";
      Com<IMFAttributes> which;
      HRESULT r = MFCreateAttributes(which.put(), 2);
      if (FAILED(r)) return hr("attributes", r);
      which->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
      which->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, toWide(deviceId).c_str());
      r = MFCreateDeviceSource(which.get(), source.put());
      if (FAILED(r)) return hr("device open (in use by another program?)", r);

      Com<IMFAttributes> options;
      r = MFCreateAttributes(options.put(), 1);
      if (FAILED(r)) return hr("attributes", r);
      // Lets the reader decode (MJPEG) and convert to the format asked for below.
      options->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);
      r = MFCreateSourceReaderFromMediaSource(source.get(), options.get(), reader.put());
      if (FAILED(r)) return hr("device reader", r);

      Com<IMFMediaType> native;
      if (!pickNative(reader.get(), native, width, height, milliFps)) return "the device offers no picture up to 1920x1080";
      r = reader->SetCurrentMediaType(kStream, nullptr, native.get());
      if (FAILED(r)) return hr("device mode", r);

      Com<IMFMediaType> wanted;
      r = MFCreateMediaType(wanted.put());
      if (FAILED(r)) return hr("format", r);
      wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
      wanted->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
      MFSetAttributeSize(wanted.get(), MF_MT_FRAME_SIZE, width, height);
      r = reader->SetCurrentMediaType(kStream, nullptr, wanted.get());
      if (FAILED(r)) return hr("conversion to NV12", r);
      r = reader->SetStreamSelection(kStream, TRUE);
      if (FAILED(r)) return hr("stream", r);
      return {};
    }();

    if (!problem.empty()) {
      reader.reset();
      if (source) source->Shutdown();
      ready.set_value(problem);
      return;
    }
    width_ = static_cast<int>(width);
    height_ = static_cast<int>(height);
    milliFps_ = static_cast<int>(milliFps);
    running_ = true;
    ready.set_value({});

    const int w = static_cast<int>(width), h = static_cast<int>(height);
    while (!stopRequested_) {
      DWORD flags = 0;
      LONGLONG time100ns = 0;
      Com<IMFSample> sample;
      const HRESULT r = reader->ReadSample(kStream, 0, nullptr, &flags, &time100ns, sample.put());
      if (FAILED(r) || (flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_ENDOFSTREAM)) != 0) break;  // device went away
      if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) != 0) break;  // the signal changed: start again
      if (!sample) continue;  // a gap in the stream
      Com<IMFMediaBuffer> buffer;
      if (FAILED(sample->ConvertToContiguousBuffer(buffer.put())) || !buffer) continue;

      // The 2D view gives the real row length; the plain view packs rows at the picture width.
      Com<IMF2DBuffer> flat;
      BYTE* first = nullptr;
      LONG pitch = 0;
      if (SUCCEEDED(buffer->QueryInterface(__uuidof(IMF2DBuffer), reinterpret_cast<void**>(flat.put()))) && flat &&
          SUCCEEDED(flat->Lock2D(&first, &pitch))) {
        if (first != nullptr && pitch >= w) {
          sink->onFrame(first, pitch, first + static_cast<std::ptrdiff_t>(pitch) * h, pitch, w, h, time100ns / 10);
        }
        flat->Unlock2D();
        continue;
      }
      BYTE* data = nullptr;
      DWORD length = 0;
      if (FAILED(buffer->Lock(&data, nullptr, &length))) continue;
      const std::size_t need = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) +
                               static_cast<std::size_t>(w + (w & 1)) * static_cast<std::size_t>((h + 1) / 2);
      if (data != nullptr && length >= need) {
        sink->onFrame(data, w, data + static_cast<std::ptrdiff_t>(w) * h, w + (w & 1), w, h, time100ns / 10);
      }
      buffer->Unlock();
    }
    reader.reset();
    source->Shutdown();
    running_ = false;
  }

  std::thread thread_;
  std::atomic<bool> stopRequested_{false};
  std::atomic<bool> running_{false};
  std::atomic<int> width_{0}, height_{0}, milliFps_{0};
};

}  // namespace

std::unique_ptr<VideoInput> makeVideoInput() { return std::make_unique<MfVideoInput>(); }

}  // namespace backhaul
