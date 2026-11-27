// Loads the sample plug-in built next to this test and runs sound through it.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "core/effect_chain.h"
#include "plugins/plugin_host.h"

namespace {

std::vector<std::uint8_t> savedWith(double gain, std::int32_t windowNote) {
  // What PluginInstance::state() gives for the sample plug-in: length, gain, window note, peak.
  std::vector<std::uint8_t> bytes{8, 0, 0, 0};
  bytes.resize(4 + 8 + 4 + 8);
  std::memcpy(bytes.data() + 4, &gain, 8);
  std::memcpy(bytes.data() + 12, &windowNote, 4);
  return bytes;
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::printf("usage: plugin_tests <path of SampleGain.vst3>\n");
    return 2;
  }
  const std::string path = argv[1];
  const std::unique_ptr<backhaul::PluginHost> host = backhaul::makePluginHost();
  CHECK(host != nullptr);
  if (host == nullptr) return 1;
  host->modules();  // whatever is installed on this machine; must simply not fail

  std::string error;
  CHECK(host->classes(path + ".missing", &error).empty() && !error.empty());
  CHECK(host->create(path + ".missing", "x", 1, &error) == nullptr);

  // The file holds two effects; the controller class is not listed.
  error.clear();
  const std::vector<backhaul::PluginClass> classes = host->classes(path, &error);
  CHECK(classes.size() == 2 && error.empty());
  if (classes.size() != 2) {
    std::printf("could not read %s: %s\n", path.c_str(), error.c_str());
    return 1;
  }
  CHECK(classes[0].name == "Sample Gain" && classes[1].name == "Sample Gain Stereo");
  CHECK(classes[0].vendor == "Sample Vendor" && classes[0].id != classes[1].id && classes[0].id.size() == 32);
  CHECK(host->create(path, "00000000000000000000000000000000", 1, &error) == nullptr && !error.empty());

  {  // one channel: level untouched at first, 32 samples late, as the plug-in reports
    const std::shared_ptr<backhaul::PluginInstance> gain = host->create(path, classes[0].id, 1, &error);
    CHECK(gain != nullptr);
    if (gain == nullptr) return 1;
    CHECK(gain->name() == "Sample Gain" && gain->latencySamples() == 32);
    std::vector<float> block(480);
    for (std::size_t i = 0; i < block.size(); ++i) block[i] = static_cast<float>(i + 1) / 1000.0f;
    float* one = block.data();
    gain->process(&one, 1, block.size());
    CHECK_NEAR(block[31], 0.0, 1e-9);
    CHECK_NEAR(block[32], 0.001, 1e-6);
    CHECK_NEAR(block[479], 0.448, 1e-6);

    // Saved settings come back: gain 1.0 doubles the level.
    CHECK(gain->setState(savedWith(1.0, 40)));
    std::vector<float> ones(4800, 0.25f);
    one = ones.data();
    gain->process(&one, 1, ones.size());  // more than one piece inside the host
    CHECK_NEAR(ones[4000], 0.5, 1e-6);
    const std::vector<std::uint8_t> saved = gain->state();
    CHECK(saved.size() == 24 && saved[0] == 8);
    double gainBack = 0.0;
    std::int32_t noteBack = 0;
    if (saved.size() == 24) {
      std::memcpy(&gainBack, saved.data() + 4, 8);
      std::memcpy(&noteBack, saved.data() + 12, 4);
    }
    CHECK_NEAR(gainBack, 1.0, 1e-12);
    CHECK(noteBack == 41);  // the window's own part went through the plug-in as well

    // What the plug-in reports back reaches its window side after idle().
    gain->idle();
    const std::vector<std::uint8_t> after = gain->state();
    double peakBack = 0.0;
    if (after.size() == 24) std::memcpy(&peakBack, after.data() + 16, 8);
    CHECK_NEAR(peakBack, 0.5, 1e-6);

    // The operator turns the knob in the plug-in's window: the sound follows.
    CHECK(gain->setState(savedWith(1.0, 1000)));
    std::fill(ones.begin(), ones.end(), 0.25f);
    gain->process(&one, 1, ones.size());
    CHECK_NEAR(ones[4000], 0.125, 1e-6);  // 0.25 on the knob halves the level

    // Nonsense is refused and changes nothing.
    CHECK(!gain->setState({1, 2}));
    CHECK(!gain->setState({200, 0, 0, 0, 1}));
    CHECK(!gain->hasEditor() || gain->openEditor("Sample") || !gain->editorOpen());
    gain->closeEditor();
    CHECK(!gain->editorOpen());

    // Sound with the wrong number of channels is left alone.
    float l[4] = {0.1f, 0.1f, 0.1f, 0.1f}, r[4] = {0.2f, 0.2f, 0.2f, 0.2f};
    float* two[2] = {l, r};
    gain->process(two, 2, 4);
    CHECK_NEAR(l[0], 0.1, 1e-6);
  }

  {  // a stereo-only plug-in on one channel, and on two
    const std::shared_ptr<backhaul::PluginInstance> wide = host->create(path, classes[1].id, 1, &error);
    CHECK(wide != nullptr);
    if (wide != nullptr) {
      CHECK(wide->setState(savedWith(0.75, 0)));
      std::vector<float> block(480, 0.2f);
      float* one = block.data();
      wide->process(&one, 1, block.size());
      CHECK_NEAR(block[400], 0.3, 1e-6);
    }
    const std::shared_ptr<backhaul::PluginInstance> both = host->create(path, classes[0].id, 2, &error);
    CHECK(both != nullptr);
    if (both != nullptr) {
      CHECK(both->setState(savedWith(1.0, 0)));
      std::vector<float> left(480, 0.1f), right(480, -0.2f);
      float* two[2] = {left.data(), right.data()};
      both->process(two, 2, 480);
      CHECK_NEAR(left[400], 0.2, 1e-6);
      CHECK_NEAR(right[400], -0.4, 1e-6);

      // In a row with a second one, through the same path the mixer uses.
      backhaul::EffectChain chain;
      chain.set({both, host->create(path, classes[1].id, 2, &error)});
      CHECK(chain.size() == 2 && chain.latencySamples() == 64);
      std::fill(left.begin(), left.end(), 0.1f);
      std::fill(right.begin(), right.end(), 0.1f);
      chain.process(two, 2, 480);
      CHECK_NEAR(left[400], 0.2, 1e-6);  // doubled by the first, untouched by the second
      chain.set({});
    }
  }

  if (testing::failures() == 0) {
    std::printf("all tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
