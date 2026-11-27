// A tiny VST 3 plug-in used only by the tests of the plug-in host. One file with two
// effects: "Sample Gain" (mono or stereo) and "Sample Gain Stereo" (stereo only).
// Each multiplies by its one parameter and delays the sound by 32 samples, which it reports.
#include <algorithm>
#include <vector>

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/vstaudioeffect.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

const FUID kGainProcessor(0x42484C31, 0x53414D50, 0x4C453031, 0x00000001);
const FUID kStereoProcessor(0x42484C31, 0x53414D50, 0x4C453031, 0x00000002);
const FUID kGainController(0x42484C31, 0x53414D50, 0x4C453031, 0x00000003);
constexpr ParamID kGainParam = 100;
constexpr ParamID kPeakParam = 101;  // read-only: what the plug-in reports back
constexpr int kDelay = 32;

class GainProcessor : public AudioEffect {
 public:
  explicit GainProcessor(bool stereoOnly) : stereoOnly_(stereoOnly) { setControllerClass(kGainController); }
  static FUnknown* createGain(void*) { return static_cast<IAudioProcessor*>(new GainProcessor(false)); }
  static FUnknown* createStereo(void*) { return static_cast<IAudioProcessor*>(new GainProcessor(true)); }

  tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE {
    const tresult result = AudioEffect::initialize(context);
    if (result != kResultOk) return result;
    addAudioInput(STR16("In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Out"), SpeakerArr::kStereo);
    return kResultOk;
  }

  tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs,
                                        int32 numOuts) SMTG_OVERRIDE {
    if (numIns != 1 || numOuts != 1 || inputs[0] != outputs[0]) return kResultFalse;
    if (inputs[0] != SpeakerArr::kStereo && (stereoOnly_ || inputs[0] != SpeakerArr::kMono)) return kResultFalse;
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
  }

  uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE { return kDelay; }

  tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE {
    if (state) {
      for (auto& line : delay_) line.assign(kDelay, 0.0f);
      at_ = 0;
    }
    return AudioEffect::setActive(state);
  }

  tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE {
    if (data.inputParameterChanges != nullptr) {
      const int32 count = data.inputParameterChanges->getParameterCount();
      for (int32 i = 0; i < count; ++i) {
        IParamValueQueue* queue = data.inputParameterChanges->getParameterData(i);
        int32 offset = 0;
        ParamValue value = 0.0;
        if (queue != nullptr && queue->getParameterId() == kGainParam &&
            queue->getPoint(queue->getPointCount() - 1, offset, value) == kResultOk) {
          gain_ = value;
        }
      }
    }
    if (data.numInputs < 1 || data.numOutputs < 1 || data.numSamples < 1) return kResultOk;
    const int32 channels = std::min<int32>(std::min(data.inputs[0].numChannels, data.outputs[0].numChannels), 2);
    const float factor = static_cast<float>(gain_ * 2.0);
    float peak = 0.0f;
    std::size_t at = at_;
    for (int32 c = 0; c < channels; ++c) {
      at = at_;
      const float* in = data.inputs[0].channelBuffers32[c];
      float* out = data.outputs[0].channelBuffers32[c];
      for (int32 i = 0; i < data.numSamples; ++i) {
        const float now = in[i] * factor;
        out[i] = delay_[c][at];
        delay_[c][at] = now;
        at = (at + 1) % kDelay;
        peak = std::max(peak, out[i] < 0.0f ? -out[i] : out[i]);
      }
    }
    at_ = at;
    if (data.outputParameterChanges != nullptr) {
      int32 index = 0;
      if (IParamValueQueue* queue = data.outputParameterChanges->addParameterData(kPeakParam, index)) {
        queue->addPoint(0, std::min(1.0f, peak), index);
      }
    }
    return kResultOk;
  }

  tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE {
    IBStreamer stream(state, kLittleEndian);
    double saved = 0.5;
    if (!stream.readDouble(saved)) return kResultFalse;
    gain_ = saved;
    return kResultOk;
  }

  tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE {
    IBStreamer stream(state, kLittleEndian);
    return stream.writeDouble(gain_) ? kResultOk : kResultFalse;
  }

 private:
  bool stereoOnly_;
  double gain_ = 0.5;  // 0.5 leaves the level as it is
  std::vector<float> delay_[2];
  std::size_t at_ = 0;
};

class GainController : public EditController {
 public:
  static FUnknown* create(void*) { return static_cast<IEditController*>(new GainController); }

  tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE {
    const tresult result = EditController::initialize(context);
    if (result != kResultOk) return result;
    parameters.addParameter(STR16("Gain"), nullptr, 0, 0.5, ParameterInfo::kCanAutomate, kGainParam);
    parameters.addParameter(STR16("Peak"), nullptr, 0, 0.0, ParameterInfo::kIsReadOnly, kPeakParam);
    return kResultOk;
  }

  tresult PLUGIN_API setComponentState(IBStream* state) SMTG_OVERRIDE {
    IBStreamer stream(state, kLittleEndian);
    double saved = 0.5;
    if (!stream.readDouble(saved)) return kResultFalse;
    setParamNormalized(kGainParam, saved);
    return kResultOk;
  }

  // The window's own part of the state: a number the tests can recognise.
  tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE {
    IBStreamer stream(state, kLittleEndian);
    return stream.writeInt32(windowNote_) && stream.writeDouble(getParamNormalized(kPeakParam)) ? kResultOk : kResultFalse;
  }
  tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE {
    IBStreamer stream(state, kLittleEndian);
    int32 saved = 0;
    if (!stream.readInt32(saved)) return kResultFalse;
    windowNote_ = saved + 1;  // shows that it went through here
    if (saved >= 1000) {  // stands in for the operator turning the knob in the plug-in's window
      setParamNormalized(kGainParam, 0.25);
      beginEdit(kGainParam);
      performEdit(kGainParam, 0.25);
      endEdit(kGainParam);
    }
    return kResultOk;
  }

 private:
  int32 windowNote_ = 7;
};

}  // namespace

BEGIN_FACTORY_DEF("Sample Vendor", "https://example.invalid", "mailto:nobody@example.invalid")
DEF_CLASS2(INLINE_UID_FROM_FUID(kGainProcessor), PClassInfo::kManyInstances, kVstAudioEffectClass, "Sample Gain",
           Vst::kDistributable, Vst::PlugType::kFx, "1.0.0", kVstVersionString, GainProcessor::createGain)
DEF_CLASS2(INLINE_UID_FROM_FUID(kStereoProcessor), PClassInfo::kManyInstances, kVstAudioEffectClass, "Sample Gain Stereo",
           Vst::kDistributable, Vst::PlugType::kFx, "1.0.0", kVstVersionString, GainProcessor::createStereo)
DEF_CLASS2(INLINE_UID_FROM_FUID(kGainController), PClassInfo::kManyInstances, kVstComponentControllerClass,
           "Sample Gain Controller", 0, "", "1.0.0", kVstVersionString, GainController::create)
END_FACTORY
