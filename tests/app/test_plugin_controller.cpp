// Plug-in rows and projects, with a made-up plug-in host (the real one is tested in tests/plugins).
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdint>
#include <map>
#include <vector>

#include "app_state.h"
#include "audio_controller.h"
#include "check.h"
#include "plugin_controller.h"
#include "project_controller.h"

namespace {

// Multiplies by a factor that is its whole "settings" (one byte: factor * 10).
struct FakePlugin final : backhaul::PluginInstance {
  std::string title;
  float factor = 2.0f;
  int delay = 0;
  bool edited = false;
  bool refuseState = false;
  int* alive;
  FakePlugin(std::string t, int* a) : title(std::move(t)), alive(a) { ++*alive; }
  ~FakePlugin() override { --*alive; }
  void process(float* const* channels, int channelCount, std::size_t frames) override {
    for (int c = 0; c < channelCount; ++c) {
      for (std::size_t i = 0; i < frames; ++i) channels[c][i] *= factor;
    }
  }
  int latencySamples() const override { return delay; }
  std::string name() const override { return title; }
  std::vector<std::uint8_t> state() override { return {static_cast<std::uint8_t>(factor * 10.0f + 0.5f)}; }
  bool setState(const std::vector<std::uint8_t>& saved) override {
    if (refuseState || saved.size() != 1) return false;
    factor = static_cast<float>(saved[0]) / 10.0f;
    return true;
  }
  bool hasEditor() const override { return false; }
  bool openEditor(const std::string&) override { return false; }
  void closeEditor() override {}
  bool editorOpen() const override { return false; }
  void idle() override {}
  bool takeEdited() override {
    const bool was = edited;
    edited = false;
    return was;
  }
};

struct FakeHost final : backhaul::PluginHost {
  std::vector<std::string> files;
  std::map<std::string, std::vector<backhaul::PluginClass>> inside;
  std::vector<FakePlugin*> made;
  int alive = 0;
  int lastChannels = 0;
  bool monoOnly = false;
  std::vector<std::string> modules() override { return files; }
  std::vector<backhaul::PluginClass> classes(const std::string& path, std::string* error) override {
    const auto it = inside.find(path);
    if (it == inside.end()) {
      *error = "not a plug-in";
      return {};
    }
    return it->second;
  }
  std::shared_ptr<backhaul::PluginInstance> create(const std::string& path, const std::string& classId, int channels,
                                                 std::string* error) override {
    lastChannels = channels;
    if (monoOnly && channels == 2) {
      *error = "the plug-in only works in mono";
      return nullptr;
    }
    for (const backhaul::PluginClass& c : inside[path]) {
      if (c.id != classId) continue;
      auto plugin = std::make_shared<FakePlugin>(c.name, &alive);
      if (c.name == "Sample Limiter") plugin->delay = 96;
      made.push_back(plugin.get());
      return plugin;
    }
    *error = "no such effect";
    return nullptr;
  }
};

struct SilentOutput final : backhaul::AudioOutput {
  bool alive = false;
  std::vector<backhaul::AudioDevice> devices() override { return {{"desk", "サンプル卓出力"}}; }
  bool start(const std::string&, backhaul::AudioMixer*, std::string*) override { return alive = true; }
  void stop() override { alive = false; }
  bool running() const override { return alive; }
  int channels() const override { return alive ? 4 : 0; }
};

struct TapMic final : backhaul::AudioInput {
  bool alive = false;
  backhaul::AudioSink* sink = nullptr;
  std::vector<backhaul::AudioDevice> devices() override { return {{"mic", "サンプル卓入力"}}; }
  bool start(const std::string&, backhaul::AudioSink* s, std::string*) override {
    sink = s;
    return alive = true;
  }
  void stop() override { alive = false; }
  bool running() const override { return alive; }
  int channels() const override { return alive ? 2 : 0; }
};

QString touch(const QTemporaryDir& dir, const char* name) {
  const QString path = dir.filePath(QString::fromLatin1(name));
  QFile file(path);
  CHECK(file.open(QIODevice::WriteOnly));
  return path;
}

FakeHost* newHost(const QString& eq, const QString& suite) {
  auto* host = new FakeHost;
  host->files = {eq.toStdString(), suite.toStdString()};
  host->inside[eq.toStdString()] = {{"eq-1", "Sample EQ", "Sample Vendor"}};
  host->inside[suite.toStdString()] = {{"comp-1", "Sample Comp", "Sample Vendor"}, {"lim-1", "Sample Limiter", "Sample Vendor"}};
  return host;
}

float slotLevel(backhaul::AudioMixer* mixer, int slot, int channel) {
  const std::vector<std::int16_t> quarter(9600, 8192);  // 0.25
  mixer->flush(slot);
  std::vector<float> out(4800 * 4);
  mixer->render(out.data(), 480, 4);  // lets the flush happen
  mixer->push(slot, quarter.data(), quarter.size(), 48000);
  mixer->render(out.data(), 4800, 4);
  return out[4000 * 4 + static_cast<std::size_t>(channel)];
}

void testRows() {
  QTemporaryDir dir;
  const QString eq = touch(dir, "SampleEQ.vst3"), suite = touch(dir, "SampleSuite.vst3");
  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("audio"));
  AudioController audio(a, std::make_unique<SilentOutput>(), nullptr, std::unique_ptr<backhaul::AudioInput>(new TapMic));
  FakeHost* host = newHost(eq, suite);
  PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host));
  int notes = 0;
  QObject::connect(&plugins, &PluginController::changed, [&] { ++notes; });

  CHECK(plugins.available() && plugins.maxPerRack() == 4);
  CHECK(plugins.moduleNames() == QStringList({QStringLiteral("SampleEQ"), QStringLiteral("SampleSuite")}));
  CHECK(plugins.micRack().isEmpty() && a.pluginSettings().isEmpty());
  CHECK_NEAR(slotLevel(audio.mixer(), 0, 0), 0.25, 1e-3);  // slot 1 plays on channel 1

  // A file with one effect goes straight in: for a slot, with one channel.
  plugins.add(0, 0);
  CHECK(host->alive == 1 && host->lastChannels == 1 && notes > 0);
  CHECK(plugins.slotRacks()[0].toList().size() == 1);
  CHECK(plugins.slotRacks()[0].toList()[0].toMap()[QStringLiteral("name")].toString() == QStringLiteral("Sample EQ"));
  CHECK(plugins.slotRacks()[1].toList().isEmpty());
  CHECK_NEAR(slotLevel(audio.mixer(), 0, 0), 0.5, 1e-3);   // doubled by the plug-in
  CHECK_NEAR(slotLevel(audio.mixer(), 1, 1), 0.25, 1e-3);  // the next person is untouched

  // Bypass, and back.
  plugins.setBypassed(0, 0, true);
  CHECK_NEAR(slotLevel(audio.mixer(), 0, 0), 0.25, 1e-3);
  CHECK(plugins.slotRacks()[0].toList()[0].toMap()[QStringLiteral("bypassed")].toBool());
  plugins.setBypassed(0, 0, false);

  // A file with several effects asks which one.
  plugins.add(0, 1);
  CHECK(host->alive == 1 && plugins.choiceTarget() == 0);
  CHECK(plugins.choices() == QStringList({QStringLiteral("Sample Comp"), QStringLiteral("Sample Limiter")}));
  plugins.choose(5);  // no such line
  CHECK(host->alive == 1);
  plugins.choose(1);
  CHECK(host->alive == 2 && plugins.choices().isEmpty() && plugins.choiceTarget() != 0);
  CHECK_NEAR(slotLevel(audio.mixer(), 0, 0), 1.0, 1e-3);  // both in a row
  CHECK_NEAR(plugins.slotDelaysMs()[0].toDouble(), 2.0, 1e-9);  // the limiter's 96 samples
  plugins.setBypassed(0, 1, true);
  CHECK_NEAR(plugins.slotDelaysMs()[0].toDouble(), 0.0, 1e-9);
  plugins.setBypassed(0, 1, false);
  plugins.add(0, 1);
  plugins.cancelChoice();
  CHECK(plugins.choices().isEmpty() && host->alive == 2);

  // At most four in one place.
  plugins.add(0, 0);
  plugins.add(0, 0);
  plugins.add(0, 0);
  CHECK(plugins.slotRacks()[0].toList().size() == 4 && host->alive == 4);

  // Taking the first one out: the others move up and keep working.
  plugins.remove(0, 0);
  plugins.remove(0, 9);  // no such line
  CHECK(host->alive == 3 && plugins.slotRacks()[0].toList().size() == 3);
  CHECK(plugins.slotRacks()[0].toList()[0].toMap()[QStringLiteral("name")].toString() == QStringLiteral("Sample Limiter"));
  plugins.remove(0, 2);
  plugins.remove(0, 1);
  CHECK(host->alive == 1);
  CHECK_NEAR(slotLevel(audio.mixer(), 0, 0), 0.5, 1e-3);

  // Towards the meeting: two channels, and the sound that is sent goes through it.
  plugins.add(PluginController::kMic, 0);
  CHECK(host->lastChannels == 2 && plugins.micRack().size() == 1);
  std::vector<std::int16_t> sent;
  audio.setSender([&](const std::int16_t* samples, std::size_t n, int channels) {
    sent.insert(sent.end(), samples, samples + n * static_cast<std::size_t>(channels));
    return true;
  });
  const std::vector<float> quarter(480 * 2, 0.25f);
  audio.micFeed()->write(quarter.data(), 480, 2);
  CHECK(audio.pumpOnce() == 1 && sent.size() == 480);
  CHECK(!sent.empty() && sent[100] == 16384);  // 0.25 doubled
  host->monoOnly = true;
  plugins.add(PluginController::kMic, 0);
  CHECK(plugins.micRack().size() == 1 && plugins.problem().contains(QStringLiteral("モノラル専用")));
  host->monoOnly = false;

  // A file that is not a plug-in is said so and changes nothing.
  host->files.push_back(touch(dir, "Broken.vst3").toStdString());
  plugins.refreshModules();
  plugins.add(1, 2);
  CHECK(plugins.slotRacks()[1].toList().isEmpty() && plugins.problem().contains(QStringLiteral("Broken")));
  plugins.add(99, 0);
  plugins.add(0, 99);

  // A person slot added later pushes the slide slot's row down, not the people's.
  const int before = static_cast<int>(plugins.slotRacks().size());
  a.addPersonSlot();
  CHECK(static_cast<int>(plugins.slotRacks().size()) == before + 1);
  CHECK(plugins.slotRacks()[0].toList().size() == 1);

  // What is kept: where each plug-in sits, and its own settings once they were captured.
  host->made.back()->factor = 3.0f;       // the mic plug-in
  host->made.back()->edited = true;       // ...because the operator turned a knob
  plugins.tick();
  CHECK(a.pluginSettings().value(QStringLiteral("mic")).toArray()[0].toObject().value(QStringLiteral("state")).isUndefined());
  for (int i = 0; i < 40; ++i) plugins.tick();  // a second of quiet
  const QJsonObject kept = a.pluginSettings();
  CHECK(kept.value(QStringLiteral("mic")).toArray().size() == 1);
  CHECK(kept.value(QStringLiteral("mic")).toArray()[0].toObject().value(QStringLiteral("state")).toString() ==
        QString::fromLatin1(QByteArray(1, char(30)).toBase64()));
  CHECK(kept.value(QStringLiteral("slots")).toArray()[0].toArray()[0].toObject().value(QStringLiteral("class")).toString() ==
        QStringLiteral("lim-1"));
  CHECK(a.settingsJson().value(QStringLiteral("plugins")).toObject() == kept);
}

void testRestoreAndGuard() {
  QTemporaryDir dir, guard;
  const QString eq = touch(dir, "SampleEQ.vst3"), suite = touch(dir, "SampleSuite.vst3");
  QJsonObject saved;
  {  // set up and save
    AppState a;
    a.loadDefaults();
    a.setRole(QStringLiteral("audio"));
    AudioController audio(a, std::make_unique<SilentOutput>());
    FakeHost* host = newHost(eq, suite);
    PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host), guard.path());
    plugins.add(0, 0);
    plugins.add(1, 1);
    plugins.choose(0);
    plugins.setBypassed(1, 0, true);
    host->made[0]->factor = 1.5f;
    CHECK(QFile::exists(guard.filePath(QStringLiteral("plugins-running"))));
    CHECK(!QFile::exists(guard.filePath(QStringLiteral("plugins-loading"))));
    plugins.capture();
    saved = a.settingsJson();
  }
  CHECK(!QFile::exists(guard.filePath(QStringLiteral("plugins-running"))));  // a clean end leaves no note

  {  // next start: everything is back, with its settings
    AppState a;
    CHECK(a.applySettings(saved));
    AudioController audio(a, std::make_unique<SilentOutput>());
    FakeHost* host = newHost(eq, suite);
    PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host), guard.path());
    plugins.restore();
    CHECK(host->alive == 2 && plugins.problem().isEmpty());
    CHECK_NEAR(host->made[0]->factor, 1.5, 1e-6);
    CHECK(plugins.slotRacks()[1].toList()[0].toMap()[QStringLiteral("bypassed")].toBool());
    CHECK_NEAR(slotLevel(audio.mixer(), 0, 0), 0.375, 1e-3);
    CHECK(a.settingsJson() == saved);  // restoring changes nothing in the file
  }

  {  // a plug-in that is not on this PC keeps its line and its settings
    QFile::remove(eq);
    AppState a;
    CHECK(a.applySettings(saved));
    AudioController audio(a, std::make_unique<SilentOutput>());
    FakeHost* host = newHost(eq, suite);
    PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host), guard.path());
    plugins.restore();
    CHECK(host->alive == 1);
    const QVariantMap line = plugins.slotRacks()[0].toList()[0].toMap();
    CHECK(!line[QStringLiteral("loaded")].toBool() && line[QStringLiteral("note")].toString().contains(QStringLiteral("見つかりません")));
    plugins.capture();
    CHECK(a.settingsJson() == saved);
    // It turns up again: one press brings it back with its settings.
    touch(dir, "SampleEQ.vst3");
    plugins.retry(0, 0);
    CHECK(host->alive == 2 && plugins.slotRacks()[0].toList()[0].toMap()[QStringLiteral("loaded")].toBool());
    CHECK_NEAR(host->made.back()->factor, 1.5, 1e-6);
  }

  {  // the program went down while loading a plug-in: that one stays out, the others run
    QFile note(guard.filePath(QStringLiteral("plugins-loading")));
    CHECK(note.open(QIODevice::WriteOnly));
    note.write(eq.toUtf8());
    note.close();
    AppState a;
    CHECK(a.applySettings(saved));
    AudioController audio(a, std::make_unique<SilentOutput>());
    FakeHost* host = newHost(eq, suite);
    PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host), guard.path());
    plugins.restore();
    CHECK(host->alive == 1 && plugins.problem().contains(QStringLiteral("SampleEQ")));
    CHECK(!plugins.slotRacks()[0].toList()[0].toMap()[QStringLiteral("loaded")].toBool());
    CHECK(a.pluginSettings().value(QStringLiteral("slots")).toArray()[0].toArray()[0].toObject().value(QStringLiteral("blocked")).toBool());
    CHECK(!QFile::exists(guard.filePath(QStringLiteral("plugins-loading"))));
    const QJsonObject blocked = a.settingsJson();

    // It stays out on later starts too, until the operator asks for it.
    AppState b;
    CHECK(b.applySettings(blocked));
    AudioController audio2(b, std::make_unique<SilentOutput>());
    FakeHost* host2 = newHost(eq, suite);
    PluginController again(b, audio2.mixer(), audio2.micEffects(), std::unique_ptr<backhaul::PluginHost>(host2), guard.path());
    again.restore();
    CHECK(host2->alive == 1);
    again.retry(0, 0);
    CHECK(host2->alive == 2);
    CHECK(b.pluginSettings().value(QStringLiteral("slots")).toArray()[0].toArray()[0].toObject().value(QStringLiteral("blocked")).isUndefined());
  }

  {  // the program did not end properly while plug-ins ran: everything opens bypassed
    QFile note(guard.filePath(QStringLiteral("plugins-running")));
    CHECK(note.open(QIODevice::WriteOnly));
    note.close();
    AppState a;
    CHECK(a.applySettings(saved));
    AudioController audio(a, std::make_unique<SilentOutput>());
    FakeHost* host = newHost(eq, suite);
    PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host), guard.path());
    plugins.restore();
    CHECK(host->alive == 2 && plugins.problem().contains(QStringLiteral("バイパス")));
    CHECK(plugins.slotRacks()[0].toList()[0].toMap()[QStringLiteral("bypassed")].toBool());
    CHECK_NEAR(slotLevel(audio.mixer(), 0, 0), 0.25, 1e-3);
  }

  {  // settings a plug-in refuses: it runs, and the line says that the old settings are not back
    AppState a;
    QJsonObject odd = saved;
    QJsonObject pluginsPart = odd.value(QStringLiteral("plugins")).toObject();
    QJsonArray slotRows = pluginsPart.value(QStringLiteral("slots")).toArray();
    QJsonArray first = slotRows[0].toArray();
    QJsonObject entry = first[0].toObject();
    entry[QStringLiteral("state")] = QString::fromLatin1(QByteArray("too long").toBase64());
    first[0] = entry;
    slotRows[0] = first;
    pluginsPart[QStringLiteral("slots")] = slotRows;
    odd[QStringLiteral("plugins")] = pluginsPart;
    CHECK(a.applySettings(odd));
    AudioController audio(a, std::make_unique<SilentOutput>());
    FakeHost* host = newHost(eq, suite);
    PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host));
    plugins.restore();
    const QVariantMap line = plugins.slotRacks()[0].toList()[0].toMap();
    CHECK(line[QStringLiteral("loaded")].toBool() && line[QStringLiteral("note")].toString().contains(QStringLiteral("戻せません")));

    // Plug-in settings of the wrong kind in the file: the whole file is refused.
    odd[QStringLiteral("plugins")] = QStringLiteral("nonsense");
    AppState c;
    c.loadDefaults();
    CHECK(!c.applySettings(odd));
  }
}

void testProject() {
  QTemporaryDir dir;
  const QString eq = touch(dir, "SampleEQ.vst3"), suite = touch(dir, "SampleSuite.vst3");
  const QString file = dir.filePath(QStringLiteral("sample-event.backhaul"));
  {
    AppState a;
    a.loadDefaults();
    a.setRole(QStringLiteral("audio"));
    a.setZoomName(0, QStringLiteral("Kakuu Ichiro"));
    a.nudgeGain(0, -6);
    AudioController audio(a, std::make_unique<SilentOutput>());
    FakeHost* host = newHost(eq, suite);
    PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host));
    int reloads = 0;
    ProjectController project(a, plugins, [&] { ++reloads; });
    plugins.add(0, 0);
    host->made[0]->factor = 0.5f;  // not captured yet: saving asks the plug-in itself
    CHECK(project.saveTo(file) && !project.failed());
    CHECK(project.fileName() == QStringLiteral("sample-event.backhaul") && project.message().contains(QStringLiteral("保存しました")));
    CHECK(!project.saveTo(dir.filePath(QStringLiteral("no-such-folder/x.backhaul"))) && project.failed());
    CHECK(reloads == 0);
  }
  {
    AppState a;
    a.loadDefaults();
    a.setRole(QStringLiteral("camera"));
    AudioController audio(a, std::make_unique<SilentOutput>());
    FakeHost* host = newHost(eq, suite);
    PluginController plugins(a, audio.mixer(), audio.micEffects(), std::unique_ptr<backhaul::PluginHost>(host));
    int reloads = 0;
    ProjectController project(a, plugins, [&] { ++reloads; });
    int edits = 0;
    QObject::connect(&a, &AppState::settingsEdited, [&] { ++edits; });

    // Not a project, a damaged one, a missing one: said so, and nothing here changes.
    const QString junk = dir.filePath(QStringLiteral("junk.backhaul"));
    QFile j(junk);
    CHECK(j.open(QIODevice::WriteOnly));
    j.write("{ \"format\": 1, \"slots\": 3 }");
    j.close();
    CHECK(!project.openFrom(junk) && project.failed());
    j.open(QIODevice::WriteOnly);
    j.write("not json at all");
    j.close();
    CHECK(!project.openFrom(junk) && project.failed());
    CHECK(!project.openFrom(dir.filePath(QStringLiteral("missing.backhaul"))) && project.failed());
    CHECK(a.role() == QStringLiteral("camera") && reloads == 0 && edits == 0 && host->alive == 0);

    CHECK(project.openFrom(file) && !project.failed());
    CHECK(a.role() == QStringLiteral("audio") && reloads == 1 && edits > 0);
    CHECK(a.slotList()->rows()[0].zoomName == QStringLiteral("Kakuu Ichiro"));
    CHECK_NEAR(a.slotList()->rows()[0].gainDb, -3.0, 1e-9);
    CHECK(host->alive == 1);
    CHECK_NEAR(host->made[0]->factor, 0.5, 1e-6);
    CHECK(project.message().contains(QStringLiteral("開きました")));

    // Opening again replaces what is loaded instead of adding to it.
    CHECK(project.openFrom(file));
    CHECK(host->alive == 1 && plugins.slotRacks()[0].toList().size() == 1);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication qt(argc, argv);
  testRows();
  testRestoreAndGuard();
  testProject();
  if (testing::failures() == 0) {
    std::printf("all tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
