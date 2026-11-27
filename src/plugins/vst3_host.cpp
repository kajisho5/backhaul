// VST 3 plug-ins, loaded with Steinberg's hosting helpers. The call order follows the
// VST 3 workflow: initialize -> arrangements -> setupProcessing -> setActive ->
// setProcessing -> process, and the reverse on the way out.
#include "plugins/plugin_host.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <thread>

#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace backhaul {
namespace {

using namespace Steinberg;
using namespace Steinberg::Vst;

constexpr double kRate = 48000.0;
constexpr int kMaxBlock = 2048;  // longer blocks are handed over in pieces
constexpr int kMaxBusChannels = 8;

// Values on their way between the window thread and the sound thread: one writer,
// one reader, nobody waits. When it is full the newest value is dropped.
class ValueQueue {
 public:
  struct Item {
    ParamID id;
    ParamValue value;
  };
  bool push(const Item& item) {
    const std::size_t w = write_.load(std::memory_order_relaxed);
    if (w - read_.load(std::memory_order_acquire) >= kSize) return false;
    items_[w % kSize] = item;
    write_.store(w + 1, std::memory_order_release);
    return true;
  }
  bool pop(Item* item) {
    const std::size_t r = read_.load(std::memory_order_relaxed);
    if (r == write_.load(std::memory_order_acquire)) return false;
    *item = items_[r % kSize];
    read_.store(r + 1, std::memory_order_release);
    return true;
  }

 private:
  static constexpr std::size_t kSize = 512;
  Item items_[kSize] = {};
  std::atomic<std::size_t> write_{0}, read_{0};
};

bool sameId(const TUID a, const TUID b) { return std::memcmp(a, b, sizeof(TUID)) == 0; }

std::vector<std::uint8_t> bytesOf(MemoryStream& stream) {
  const auto* data = reinterpret_cast<const std::uint8_t*>(stream.getData());
  return std::vector<std::uint8_t>(data, data + stream.getSize());
}

class Vst3Instance final : public PluginInstance, public IComponentHandler, public IPlugFrame {
 public:
  Vst3Instance(VST3::Hosting::Module::Ptr module, std::string name) : module_(std::move(module)), name_(std::move(name)) {}

  ~Vst3Instance() override {
    closeEditor();
    pauseSound();
    if (processor_ != nullptr && started_) processor_->setProcessing(false);
    if (component_ != nullptr && active_) component_->setActive(false);
    if (controller_ != nullptr) controller_->setComponentHandler(nullptr);
    processor_ = nullptr;
    component_ = nullptr;
    controller_ = nullptr;
    provider_ = nullptr;  // disconnects and terminates the plug-in
  }

  bool start(const VST3::Hosting::ClassInfo& info, int channels, std::string* error) {
    provider_ = owned(new PlugProvider(module_->getFactory(), info, true));
    if (!provider_->initialize()) {
      *error = "the plug-in could not be initialised";
      return false;
    }
    component_ = provider_->getComponentPtr();
    controller_ = provider_->getControllerPtr();
    processor_ = FUnknownPtr<IAudioProcessor>(component_);
    if (component_ == nullptr || processor_ == nullptr) {
      *error = "the plug-in does not process sound";
      return false;
    }
    if (processor_->canProcessSampleSize(kSample32) != kResultOk) {
      *error = "the plug-in does not take 32-bit samples";
      return false;
    }
    inBuses_ = component_->getBusCount(kAudio, kInput);
    outBuses_ = component_->getBusCount(kAudio, kOutput);
    if (inBuses_ < 1 || outBuses_ < 1) {
      *error = "the plug-in is not an effect (it has no sound input or no sound output)";
      return false;
    }

    // Ask for our channel count on the main buses, keep what the others have.
    hostChannels_ = channels == 2 ? 2 : 1;
    const SpeakerArrangement wanted = hostChannels_ == 2 ? SpeakerArr::kStereo : SpeakerArr::kMono;
    std::vector<SpeakerArrangement> ins(static_cast<std::size_t>(inBuses_)), outs(static_cast<std::size_t>(outBuses_));
    for (int32 i = 0; i < inBuses_; ++i) processor_->getBusArrangement(kInput, i, ins[static_cast<std::size_t>(i)]);
    for (int32 i = 0; i < outBuses_; ++i) processor_->getBusArrangement(kOutput, i, outs[static_cast<std::size_t>(i)]);
    ins[0] = wanted;
    outs[0] = wanted;
    processor_->setBusArrangements(ins.data(), inBuses_, outs.data(), outBuses_);  // a refusal shows below
    SpeakerArrangement in = 0, out = 0;
    processor_->getBusArrangement(kInput, 0, in);
    processor_->getBusArrangement(kOutput, 0, out);
    inChannels_ = SpeakerArr::getChannelCount(in);
    outChannels_ = SpeakerArr::getChannelCount(out);
    if (inChannels_ < 1 || outChannels_ < 1 || inChannels_ > kMaxBusChannels || outChannels_ > kMaxBusChannels) {
      *error = "the plug-in's channel layout cannot be used";
      return false;
    }
    if (hostChannels_ == 2 && (inChannels_ < 2 || outChannels_ < 2)) {
      *error = "the plug-in only works in mono";
      return false;
    }
    // Only the main buses carry sound; side chains and extra outputs stay off.
    for (int32 i = 0; i < inBuses_; ++i) component_->activateBus(kAudio, kInput, i, i == 0);
    for (int32 i = 0; i < outBuses_; ++i) component_->activateBus(kAudio, kOutput, i, i == 0);

    inBus_.assign(static_cast<std::size_t>(inBuses_), AudioBusBuffers{});
    outBus_.assign(static_cast<std::size_t>(outBuses_), AudioBusBuffers{});
    for (int c = 0; c < kMaxBusChannels; ++c) {
      inData_[c].assign(kMaxBlock, 0.0f);
      outData_[c].assign(kMaxBlock, 0.0f);
      inPtr_[c] = inData_[c].data();
      outPtr_[c] = outData_[c].data();
    }
    inBus_[0].numChannels = inChannels_;
    inBus_[0].channelBuffers32 = inPtr_;
    outBus_[0].numChannels = outChannels_;
    outBus_[0].channelBuffers32 = outPtr_;

    if (controller_ != nullptr) {
      controller_->setComponentHandler(this);
      paramsIn_.setMaxParameters(controller_->getParameterCount());
      paramsOut_.setMaxParameters(controller_->getParameterCount());
    }
    return activate(error);
  }

  // ---- PluginInstance ----
  std::string name() const override { return name_; }

  std::vector<std::uint8_t> state() override {
    // Layout: four bytes with the length of the processor's part, that part, then the window's part.
    auto processorPart = owned(new MemoryStream);
    if (component_ == nullptr || component_->getState(processorPart) != kResultOk) return {};
    std::vector<std::uint8_t> saved = bytesOf(*processorPart);
    const std::uint32_t length = static_cast<std::uint32_t>(saved.size());
    std::vector<std::uint8_t> all{static_cast<std::uint8_t>(length & 0xff), static_cast<std::uint8_t>((length >> 8) & 0xff),
                                  static_cast<std::uint8_t>((length >> 16) & 0xff),
                                  static_cast<std::uint8_t>((length >> 24) & 0xff)};
    all.insert(all.end(), saved.begin(), saved.end());
    auto windowPart = owned(new MemoryStream);
    if (controller_ != nullptr && controller_->getState(windowPart) == kResultOk) {
      const std::vector<std::uint8_t> more = bytesOf(*windowPart);
      all.insert(all.end(), more.begin(), more.end());
    }
    return all;
  }

  bool setState(const std::vector<std::uint8_t>& saved) override {
    if (component_ == nullptr || saved.size() < 4) return false;
    const std::size_t length = static_cast<std::size_t>(saved[0]) | (static_cast<std::size_t>(saved[1]) << 8) |
                               (static_cast<std::size_t>(saved[2]) << 16) | (static_cast<std::size_t>(saved[3]) << 24);
    if (length > saved.size() - 4) return false;
    // The streams only read; the SDK's interface is not const.
    auto* bytes = const_cast<std::uint8_t*>(saved.data());
    auto processorPart = owned(new MemoryStream(bytes + 4, static_cast<TSize>(length)));
    if (component_->setState(processorPart) != kResultOk) return false;
    if (controller_ != nullptr) {
      auto again = owned(new MemoryStream(bytes + 4, static_cast<TSize>(length)));
      controller_->setComponentState(again);
      const std::size_t rest = saved.size() - 4 - length;
      if (rest > 0) {
        auto windowPart = owned(new MemoryStream(bytes + 4 + length, static_cast<TSize>(rest)));
        controller_->setState(windowPart);
      }
    }
    return true;
  }

  int latencySamples() const override { return latency_.load(std::memory_order_relaxed); }

  void idle() override {
    ValueQueue::Item item;
    while (toWindow_.pop(&item)) {
      if (controller_ != nullptr) controller_->setParamNormalized(item.id, item.value);
    }
    if (restartWanted_.exchange(false)) reactivate();
  }

  // ---- sound thread ----
  void process(float* const* channels, int channelCount, std::size_t frames) override {
    if (channels == nullptr || channelCount != hostChannels_) return;
    inSound_.store(true);
    if (paused_.load()) {  // the window thread is restarting the plug-in: sound passes unchanged
      inSound_.store(false);
      return;
    }
    std::size_t done = 0;
    while (done < frames) {
      const std::size_t n = std::min(frames - done, static_cast<std::size_t>(kMaxBlock));
      for (int c = 0; c < inChannels_; ++c) {
        // One channel into a wider plug-in: the same sound on its first two channels, silence on the rest.
        const int from = c < channelCount ? c : (channelCount == 1 && c == 1 ? 0 : -1);
        if (from >= 0) {
          std::memcpy(inPtr_[c], channels[from] + done, n * sizeof(float));
        } else {
          std::fill(inPtr_[c], inPtr_[c] + n, 0.0f);
        }
      }
      for (int c = 0; c < outChannels_; ++c) std::fill(outPtr_[c], outPtr_[c] + n, 0.0f);

      paramsIn_.clearQueue();
      ValueQueue::Item item;
      while (toSound_.pop(&item)) {
        int32 index = 0;
        if (IParamValueQueue* queue = paramsIn_.addParameterData(item.id, index)) queue->addPoint(0, item.value, index);
      }
      paramsOut_.clearQueue();
      events_.clear();

      ProcessContext context{};
      context.sampleRate = kRate;
      context.projectTimeSamples = position_;
      context.state = ProcessContext::kPlaying;

      ProcessData data{};
      data.processMode = kRealtime;
      data.symbolicSampleSize = kSample32;
      data.numSamples = static_cast<int32>(n);
      data.numInputs = inBuses_;
      data.numOutputs = outBuses_;
      data.inputs = inBus_.data();
      data.outputs = outBus_.data();
      data.inputParameterChanges = &paramsIn_;
      data.outputParameterChanges = &paramsOut_;
      data.inputEvents = &events_;
      data.processContext = &context;
      inBus_[0].silenceFlags = 0;
      outBus_[0].silenceFlags = 0;
      if (processor_->process(data) == kResultOk) {
        for (int c = 0; c < channelCount; ++c) std::memcpy(channels[c] + done, outPtr_[c], n * sizeof(float));
      }
      // What the plug-in reports back (meters and the like) goes to its window.
      const int32 changed = paramsOut_.getParameterCount();
      for (int32 i = 0; i < changed; ++i) {
        IParamValueQueue* queue = paramsOut_.getParameterData(i);
        const int32 points = queue != nullptr ? queue->getPointCount() : 0;
        int32 offset = 0;
        ParamValue value = 0.0;
        if (points > 0 && queue->getPoint(points - 1, offset, value) == kResultOk) toWindow_.push({queue->getParameterId(), value});
      }
      position_ += static_cast<TSamples>(n);
      done += n;
    }
    inSound_.store(false);
  }

  // ---- the plug-in's window ----
  bool hasEditor() const override {
#ifdef _WIN32
    return controller_ != nullptr;
#else
    return false;  // the program runs on Windows; elsewhere plug-ins work without their window
#endif
  }

  bool editorOpen() const override { return view_ != nullptr; }

#ifdef _WIN32
  bool openEditor(const std::string& title) override {
    if (view_ != nullptr) {
      SetForegroundWindow(window_);
      return true;
    }
    if (controller_ == nullptr) return false;
    IPtr<IPlugView> view = owned(controller_->createView(ViewType::kEditor));
    if (view == nullptr || view->isPlatformTypeSupported(kPlatformTypeHWND) != kResultOk) return false;

    static const wchar_t* const kClass = L"BackhaulPluginWindow";
    static bool registered = false;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!registered) {
      WNDCLASSW wc{};
      wc.lpfnWndProc = &Vst3Instance::windowProc;
      wc.hInstance = instance;
      wc.lpszClassName = kClass;
      wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
      wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
      if (RegisterClassW(&wc) == 0) return false;
      registered = true;
    }
    ViewRect size;
    if (view->getSize(&size) != kResultOk || size.getWidth() <= 0 || size.getHeight() <= 0) size = ViewRect(0, 0, 640, 400);
    resizable_ = view->canResize() == kResultTrue;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | (resizable_ ? WS_THICKFRAME : 0);
    RECT outer{0, 0, size.getWidth(), size.getHeight()};
    AdjustWindowRectEx(&outer, style, FALSE, 0);
    const int wide = MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, nullptr, 0);
    std::wstring caption(static_cast<std::size_t>(wide > 0 ? wide : 1), L'\0');
    if (wide > 0) MultiByteToWideChar(CP_UTF8, 0, title.c_str(), -1, caption.data(), wide);
    window_ = CreateWindowExW(0, kClass, caption.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT, outer.right - outer.left,
                              outer.bottom - outer.top, nullptr, nullptr, instance, this);
    if (window_ == nullptr) return false;
    view->setFrame(this);
    if (auto scale = FUnknownPtr<IPlugViewContentScaleSupport>(view)) {
      scale->setContentScaleFactor(static_cast<float>(GetDpiForWindow(window_)) / 96.0f);
    }
    if (view->attached(window_, kPlatformTypeHWND) != kResultOk) {
      view->setFrame(nullptr);
      DestroyWindow(window_);
      window_ = nullptr;
      return false;
    }
    view_ = view;
    ShowWindow(window_, SW_SHOWNORMAL);
    return true;
  }

  void closeEditor() override {
    if (view_ != nullptr) {
      view_->setFrame(nullptr);
      view_->removed();
      view_ = nullptr;
    }
    if (window_ != nullptr) {
      SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
      DestroyWindow(window_);
      window_ = nullptr;
    }
  }

  static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
      SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* self = reinterpret_cast<Vst3Instance*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (self != nullptr) {
      if (message == WM_CLOSE) {
        self->closeEditor();
        return 0;
      }
      if (message == WM_SIZE && self->view_ != nullptr && self->resizable_ && !self->resizing_) {
        ViewRect inner(0, 0, LOWORD(lparam), HIWORD(lparam));
        self->view_->onSize(&inner);
      }
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  // IPlugFrame: the plug-in asks for another size of its window.
  tresult PLUGIN_API resizeView(IPlugView* view, ViewRect* newSize) override {
    if (view == nullptr || newSize == nullptr || window_ == nullptr) return kInvalidArgument;
    RECT outer{0, 0, newSize->getWidth(), newSize->getHeight()};
    AdjustWindowRectEx(&outer, static_cast<DWORD>(GetWindowLongPtrW(window_, GWL_STYLE)), FALSE, 0);
    resizing_ = true;
    SetWindowPos(window_, nullptr, 0, 0, outer.right - outer.left, outer.bottom - outer.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    resizing_ = false;
    return view->onSize(newSize);
  }
#else
  bool openEditor(const std::string&) override { return false; }
  void closeEditor() override {}
  tresult PLUGIN_API resizeView(IPlugView*, ViewRect*) override { return kNotImplemented; }
#endif

  // ---- IComponentHandler: the plug-in's window tells us what the operator did ----
  tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
  tresult PLUGIN_API performEdit(ParamID id, ParamValue value) override {
    toSound_.push({id, value});
    return kResultOk;
  }
  tresult PLUGIN_API endEdit(ParamID) override { return kResultOk; }
  tresult PLUGIN_API restartComponent(int32 flags) override {
    // A changed delay (or anything that needs a restart) is picked up in idle(), not from
    // inside the plug-in's own call.
    if ((flags & (kLatencyChanged | kReloadComponent)) != 0) restartWanted_.store(true);
    return kResultOk;
  }

  // ---- FUnknown: this object is owned by the program, not counted by the plug-in ----
  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
    if (obj == nullptr) return kInvalidArgument;
    if (sameId(iid, IComponentHandler::iid) || sameId(iid, FUnknown::iid)) {
      *obj = static_cast<IComponentHandler*>(this);
      return kResultOk;
    }
    if (sameId(iid, IPlugFrame::iid)) {
      *obj = static_cast<IPlugFrame*>(this);
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }

 private:
  bool activate(std::string* error) {
    ProcessSetup setup{kRealtime, kSample32, kMaxBlock, kRate};
    if (processor_->setupProcessing(setup) != kResultOk) {
      if (error != nullptr) *error = "the plug-in does not work at 48 kHz";
      return false;
    }
    if (component_->setActive(true) != kResultOk) {
      if (error != nullptr) *error = "the plug-in could not be switched on";
      return false;
    }
    active_ = true;
    latency_.store(static_cast<int>(processor_->getLatencySamples()), std::memory_order_relaxed);
    processor_->setProcessing(true);  // some plug-ins answer "not implemented"; that is fine
    started_ = true;
    paused_.store(false);
    return true;
  }

  // Makes sure the sound thread is not inside the plug-in and stays out.
  void pauseSound() {
    paused_.store(true);
    while (inSound_.load()) std::this_thread::sleep_for(std::chrono::microseconds(200));
  }

  void reactivate() {
    pauseSound();
    if (started_) processor_->setProcessing(false);
    if (active_) component_->setActive(false);
    started_ = false;
    active_ = false;
    activate(nullptr);  // on failure the plug-in stays paused: sound passes unchanged
  }

  VST3::Hosting::Module::Ptr module_;  // keeps the plug-in file loaded
  std::string name_;
  IPtr<PlugProvider> provider_;
  IPtr<IComponent> component_;
  IPtr<IEditController> controller_;
  IPtr<IAudioProcessor> processor_;
  IPtr<IPlugView> view_;
#ifdef _WIN32
  HWND window_ = nullptr;
  bool resizable_ = false;
  bool resizing_ = false;
#endif

  int hostChannels_ = 1;
  int32 inBuses_ = 0, outBuses_ = 0;
  int32 inChannels_ = 0, outChannels_ = 0;
  std::vector<AudioBusBuffers> inBus_, outBus_;
  std::vector<float> inData_[kMaxBusChannels], outData_[kMaxBusChannels];
  float* inPtr_[kMaxBusChannels] = {};
  float* outPtr_[kMaxBusChannels] = {};
  ParameterChanges paramsIn_, paramsOut_;
  EventList events_;
  TSamples position_ = 0;

  bool active_ = false, started_ = false;
  std::atomic<int> latency_{0};
  std::atomic<bool> paused_{true}, inSound_{false}, restartWanted_{false};
  ValueQueue toSound_, toWindow_;
};

class Vst3Host final : public PluginHost {
 public:
  Vst3Host() {
    // Plug-ins keep this for as long as they are loaded, which can be longer than this object.
    static HostApplication* const context = new HostApplication;
    PluginContextFactory::instance().setPluginContext(context);
  }

  std::vector<std::string> modules() override {
    std::vector<std::string> paths = VST3::Hosting::Module::getModulePaths();
    std::sort(paths.begin(), paths.end());
    return paths;
  }

  std::vector<PluginClass> classes(const std::string& path, std::string* error) override {
    std::vector<PluginClass> found;
    const VST3::Hosting::Module::Ptr module = load(path, error);
    if (module == nullptr) return found;
    for (const auto& info : module->getFactory().classInfos()) {
      if (info.category() == kVstAudioEffectClass) found.push_back({info.ID().toString(), info.name(), info.vendor()});
    }
    if (found.empty() && error != nullptr) *error = "the file holds no sound effect";
    return found;
  }

  std::shared_ptr<PluginInstance> create(const std::string& path, const std::string& classId, int channels,
                                         std::string* error) override {
    std::string reason;
    const VST3::Hosting::Module::Ptr module = load(path, &reason);
    if (module != nullptr) {
      reason = "the effect is not in this file";
      for (const auto& info : module->getFactory().classInfos()) {
        if (info.category() != kVstAudioEffectClass || info.ID().toString() != classId) continue;
        auto instance = std::make_shared<Vst3Instance>(module, info.name());
        if (instance->start(info, channels, &reason)) return instance;
        break;
      }
    }
    if (error != nullptr) *error = reason;
    return nullptr;
  }

 private:
  VST3::Hosting::Module::Ptr load(const std::string& path, std::string* error) {
    const auto known = modules_.find(path);
    if (known != modules_.end()) return known->second;
    std::string reason;
    VST3::Hosting::Module::Ptr module = VST3::Hosting::Module::create(path, reason);
    if (module == nullptr) {
      if (error != nullptr) *error = reason.empty() ? "the file could not be loaded" : reason;
      return nullptr;
    }
    modules_[path] = module;
    return module;
  }

  std::map<std::string, VST3::Hosting::Module::Ptr> modules_;  // loaded files stay loaded
};

}  // namespace

std::unique_ptr<PluginHost> makePluginHost() { return std::make_unique<Vst3Host>(); }

}  // namespace backhaul
