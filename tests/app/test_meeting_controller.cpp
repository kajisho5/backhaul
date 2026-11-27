#include <QCoreApplication>
#include <QGuiApplication>
#include <cmath>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#include "app_state.h"
#include "audio_controller.h"
#include "check.h"
#include "frame_store.h"
#include "meeting_controller.h"

namespace {

using backhaul::MeetingState;
using backhaul::Participant;
using backhaul::VideoSize;

// Stands in for the meeting library: records what it is asked and lets the test
// play the part of Zoom.
class ScriptedClient final : public backhaul::MeetingClient {
 public:
  bool join(const backhaul::JoinRequest& request, backhaul::MeetingEvents* sink) override {
    lastRequest = request;
    events = sink;
    ++joins;
    events->onMeetingState(MeetingState::Authenticating, {});
    return true;
  }
  void leave() override {
    ++leaves;
    events->onMeetingState(MeetingState::Idle, {});
  }
  void setVideoWants(const std::vector<backhaul::VideoWant>& w) override { wants = w; }
  void poll() override { ++polls; }

  backhaul::JoinRequest lastRequest;
  backhaul::MeetingEvents* events = nullptr;
  std::vector<backhaul::VideoWant> wants;
  int joins = 0, leaves = 0, polls = 0;
};

class MemoryCredentials final : public CredentialStore {
 public:
  explicit MemoryCredentials(ZoomCredentials* shared) : shared_(shared) {}
  bool load(ZoomCredentials& out) override {
    if (shared_->clientId.isEmpty()) return false;
    out = *shared_;
    return true;
  }
  bool save(const ZoomCredentials& c) override {
    *shared_ = c;
    return true;
  }
  bool clear() override {
    *shared_ = {};
    return true;
  }

 private:
  ZoomCredentials* shared_;
};

Participant person(std::uint32_t id, const char* name, bool video = true, bool muted = false) {
  Participant p;
  p.id = id;
  p.name = name;
  p.videoOn = video;
  p.audioMuted = muted;
  return p;
}

QVariant slotData(AppState& app, int row, int role) { return app.slotList()->data(app.slotList()->index(row), role); }

// A grey picture of the given size.
void sendFrame(backhaul::MeetingEvents* events, std::uint32_t user, int w, int h) {
  const std::vector<std::uint8_t> y(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), 126);
  const std::vector<std::uint8_t> c(static_cast<std::size_t>((w + 1) / 2) * static_cast<std::size_t>((h + 1) / 2), 128);
  backhaul::VideoFrame f;
  f.y = y.data();
  f.u = c.data();
  f.v = c.data();
  f.width = w;
  f.height = h;
  events->onVideoFrame(user, f);
}

}  // namespace

int main(int argc, char* argv[]) {
  QGuiApplication qtApp(argc, argv);

  CHECK(MeetingController::parseMeetingNumber(QStringLiteral("123 4567 8901")) == 12345678901ULL);
  CHECK(MeetingController::parseMeetingNumber(QStringLiteral("123-456-789")) == 123456789ULL);
  CHECK(MeetingController::parseMeetingNumber(QStringLiteral("12345678")) == 0);        // too short
  CHECK(MeetingController::parseMeetingNumber(QStringLiteral("123456789012")) == 0);    // too long
  CHECK(MeetingController::parseMeetingNumber(QStringLiteral("123 abc 8901")) == 0);
  CHECK(MeetingController::parseMeetingNumber(QStringLiteral("１２３４５６７８９")) == 0);  // full-width digits are not an ID

  {  // a build without the meeting library: nothing can be joined, nothing crashes
    AppState app;
    app.loadDefaults();
    FrameStore frames;
    MeetingController m(app, frames, nullptr, nullptr);
    CHECK(!m.available() && !m.credentialsSupported() && !m.canJoin());
    m.join();
    m.leave();
    CHECK(!m.saveCredentials(QStringLiteral("a"), QStringLiteral("b")));
    CHECK(m.statusText() == QStringLiteral("未接続"));
  }

  AppState app;
  app.loadDefaults();
  app.setZoomName(0, QStringLiteral("Kakuu Ichiro"));
  app.setZoomName(1, QStringLiteral("Mihon Hanako"));
  FrameStore frames;
  ZoomCredentials saved;
  auto* client = new ScriptedClient;
  MeetingController m(app, frames, std::unique_ptr<backhaul::MeetingClient>(client),
                      std::make_unique<MemoryCredentials>(&saved));

  // Joining needs credentials, a meeting ID and a name.
  CHECK(!m.canJoin());
  CHECK(!m.saveCredentials(QStringLiteral("  "), QStringLiteral("x")));
  CHECK(m.saveCredentials(QStringLiteral(" SampleClientId_01 "), QStringLiteral("sample-secret")));
  CHECK(saved.clientId == QStringLiteral("SampleClientId_01"));
  CHECK(m.credentialHint() == QStringLiteral("Samp…d_01"));
  CHECK(!m.canJoin());
  app.setMeetingId(QStringLiteral("123 4567 8901"));
  CHECK(m.canJoin());
  app.setJoinName(QStringLiteral("  "));
  CHECK(!m.canJoin());
  app.setJoinName(QStringLiteral("会場PC"));
  m.setProperty("passcode", QStringLiteral("sample-pass"));

  m.join();
  CHECK(client->joins == 1);
  CHECK(client->lastRequest.clientId == "SampleClientId_01");
  CHECK(client->lastRequest.clientSecret == "sample-secret");
  CHECK(client->lastRequest.meetingNumber == 12345678901ULL);
  CHECK(client->lastRequest.displayName == QStringLiteral("会場PC").toStdString());
  CHECK(client->lastRequest.passcode == "sample-pass");
  CHECK(m.busy() && m.step() == 0 && !m.canJoin());
  m.join();  // a second press while joining does nothing
  CHECK(client->joins == 1);

  client->events->onMeetingState(MeetingState::Joining, {});
  CHECK(m.step() == 1);
  client->events->onMeetingState(MeetingState::WaitingForPermission, {});
  client->events->onMeetingTopic("サンプル発表会");
  CHECK(m.step() == 2 && app.property("connected").toBool());
  CHECK(app.property("meetingName").toString() == QStringLiteral("サンプル発表会"));

  // People arrive: the two whose Zoom names are set up go straight into their slots.
  Participant self = person(1, "会場PC");
  self.isSelf = true;
  Participant host = person(2, "ホストPC");
  host.isHost = true;
  client->events->onParticipants({self, host, person(10, "Kakuu Ichiro"), person(11, "Mihon Hanako", false, true),
                                  person(12, "Shiken Goro")});
  CHECK(app.participantCount() == 5);
  CHECK(slotData(app, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  CHECK(slotData(app, 1, SlotModel::StateRole).toString() == QStringLiteral("camoff"));
  CHECK(slotData(app, 1, SlotModel::AlsoMutedRole).toBool());
  CHECK(slotData(app, 2, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
  // Two people on screen: 720p each (the limit is two streams at 720p).
  CHECK(client->wants.size() == 2);
  CHECK(client->wants[0].userId == 10 && client->wants[0].size == VideoSize::P720);
  CHECK(client->wants[1].userId == 11 && client->wants[1].size == VideoSize::P720);

  // A third person is put into slot 3 by hand: three people means 360p.
  app.setSelected(2);
  app.assignToSelected(4);
  CHECK(slotData(app, 2, SlotModel::ZoomNameRole).toString() == QStringLiteral("Shiken Goro"));
  CHECK(slotData(app, 2, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  CHECK(client->wants.size() == 3 && client->wants[2].size == VideoSize::P360);

  // Pictures: only for people who are in a slot, converted and counted.
  client->events->onMeetingState(MeetingState::Receiving, {});
  CHECK(m.step() == 3);
  sendFrame(client->events, 10, 640, 360);
  sendFrame(client->events, 10, 640, 360);
  sendFrame(client->events, 2, 640, 360);  // the host is in no slot
  CHECK(frames.latest(0).size() == QSize(640, 360));
  CHECK(qRed(frames.latest(0).pixel(5, 5)) > 120 && qRed(frames.latest(0).pixel(5, 5)) < 136);
  CHECK(frames.latest(1).isNull());
  CHECK(frames.takeCount(0) == 2 && frames.takeCount(0) == 0);

  // Voices go to the mixer under the slot of the person speaking.
  backhaul::AudioMixer mixer(4);
  m.setMixer(&mixer);
  {
    const std::vector<std::int16_t> loud(480, 16384);
    client->events->onAudio(11, loud.data(), loud.size(), 48000, 1);  // Mihon Hanako is in slot 2
    client->events->onAudio(2, loud.data(), loud.size(), 48000, 1);   // the host is in no slot
    CHECK_NEAR(mixer.takePeak(1), 0.5, 0.001);
    CHECK_NEAR(mixer.takePeak(0), 0.0, 1e-9);
  }

  // Someone drops out: the slot shows "lost" and frees its stream; the others get larger pictures.
  client->events->onParticipants({self, host, person(10, "Kakuu Ichiro"), person(12, "Shiken Goro")});
  CHECK(slotData(app, 1, SlotModel::StateRole).toString() == QStringLiteral("lost"));
  CHECK(client->wants.size() == 2 && client->wants[0].size == VideoSize::P720);
  // They come back with a new number and return to the same slot on their own.
  client->events->onParticipants({self, host, person(10, "Kakuu Ichiro"), person(12, "Shiken Goro"),
                                  person(31, "Mihon Hanako")});
  CHECK(slotData(app, 1, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  CHECK(client->wants.size() == 3);

  // "720p at most" with a single person.
  client->events->onParticipants({self, host, person(10, "Kakuu Ichiro")});
  CHECK(client->wants.size() == 1 && client->wants[0].size == VideoSize::P1080);
  app.setQualityAuto(false);
  CHECK(client->wants.size() == 1 && client->wants[0].size == VideoSize::P720);

  // Leaving clears the meeting from the screen but keeps the names for next time.
  m.leave();
  CHECK(client->leaves == 1);
  CHECK(!m.busy() && !app.property("connected").toBool());
  CHECK(app.participantCount() == 0);
  CHECK(slotData(app, 0, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
  CHECK(slotData(app, 0, SlotModel::ZoomNameRole).toString() == QStringLiteral("Kakuu Ichiro"));
  CHECK(frames.latest(0).isNull());
  CHECK(client->wants.empty());
  CHECK(m.canJoin());

  // A failure is shown with its reason and joining is possible again.
  m.join();
  client->events->onMeetingState(MeetingState::Failed, "会議に入れませんでした (4)");
  CHECK(m.failed() && !m.busy() && m.canJoin());
  CHECK(m.detail() == QStringLiteral("会議に入れませんでした (4)"));

  m.clearCredentials();
  CHECK(!m.credentialsSaved() && saved.clientId.isEmpty() && !m.canJoin());

  {  // sound devices
    struct FakeOutput final : backhaul::AudioOutput {
      std::vector<backhaul::AudioDevice> list{{"id-a", "サンプル音声機器A"}, {"id-b", "サンプル音声機器B"}};
      std::string started;
      bool alive = false;
      bool refuse = false;
      std::vector<backhaul::AudioDevice> devices() override { return list; }
      bool start(const std::string& id, backhaul::AudioMixer*, std::string* error) override {
        if (refuse) {
          *error = "busy";
          alive = false;
          return false;
        }
        started = id;
        alive = true;
        return true;
      }
      void stop() override { alive = false; }
      bool running() const override { return alive; }
      int channels() const override { return alive ? 16 : 0; }
    };

    AppState a;
    a.loadDefaults();
    auto* out = new FakeOutput;
    AudioController audio(a, std::unique_ptr<backhaul::AudioOutput>(out));
    CHECK(audio.available());
    CHECK(audio.deviceNames().size() == 3 && audio.deviceIndex() == 0);
    audio.startSaved();  // nothing saved: stays silent
    CHECK(!out->alive);

    audio.chooseDevice(2);
    CHECK(out->started == "id-b" && audio.deviceIndex() == 2);
    CHECK(a.property("audioOk").toBool());
    CHECK(a.property("audioChannels").toInt() == 16);
    CHECK(a.property("audioDeviceName").toString() == QStringLiteral("サンプル音声機器B"));

    // The mixer follows the slots: slot 1 on channel 1 by default, then moved to channel 3 at -6 dB.
    const std::vector<std::int16_t> half(4800, 16384);
    std::vector<float> block(480 * 4);
    audio.mixer()->push(0, half.data(), half.size(), 48000);
    audio.mixer()->render(block.data(), 480, 4);
    CHECK_NEAR(block[100 * 4 + 0], 0.5, 1e-3);
    CHECK_NEAR(block[100 * 4 + 2], 0.0, 1e-9);
    a.toggleRoute(0, 1);
    a.toggleRoute(0, 3);
    a.nudgeGain(0, -12);
    audio.mixer()->render(block.data(), 480, 4);
    CHECK_NEAR(block[100 * 4 + 0], 0.0, 1e-9);
    CHECK_NEAR(block[100 * 4 + 2], 0.5 * std::pow(10.0, -6.0 / 20.0), 1e-3);

    // The saved device is started again next time; a missing one is reported.
    {
      AppState b;
      CHECK(b.applySettings(a.settingsJson()));
      auto* out2 = new FakeOutput;
      AudioController again(b, std::unique_ptr<backhaul::AudioOutput>(out2));
      again.startSaved();
      CHECK(out2->started == "id-b" && again.deviceIndex() == 2 && again.problem().isEmpty());
    }
    {
      AppState b;
      CHECK(b.applySettings(a.settingsJson()));
      auto* out2 = new FakeOutput;
      out2->list.pop_back();
      AudioController again(b, std::unique_ptr<backhaul::AudioOutput>(out2));
      again.startSaved();
      CHECK(!out2->alive && again.deviceIndex() == 0 && !again.problem().isEmpty());
      CHECK(b.property("audioDeviceName").toString() == QStringLiteral("サンプル音声機器B"));  // the wish is kept
    }

    // A device that refuses, and one that is switched off.
    out->refuse = true;
    audio.chooseDevice(1);
    CHECK(audio.deviceIndex() == 0 && !a.property("audioOk").toBool() && audio.problem().contains(QStringLiteral("busy")));
    out->refuse = false;
    audio.chooseDevice(1);
    CHECK(audio.deviceIndex() == 1 && audio.problem().isEmpty());
    audio.chooseDevice(0);
    CHECK(!out->alive && a.property("audioDeviceName").toString().isEmpty());
    audio.chooseDevice(9);  // not in the list
    CHECK(audio.deviceIndex() == 0);
    CHECK(!audio.asioAvailable());
    audio.setAsio(true);  // not built in: stays on the system output
    CHECK(!a.audioAsio());
  }

  {  // ASIO next to the system output: each has its own list, and switching stops the other
    struct NamedOutput final : backhaul::AudioOutput {
      explicit NamedOutput(std::string n) : name(std::move(n)) {}
      std::string name;
      bool alive = false;
      std::vector<backhaul::AudioDevice> devices() override { return {{name + "-id", name}}; }
      bool start(const std::string&, backhaul::AudioMixer*, std::string*) override {
        alive = true;
        return true;
      }
      void stop() override { alive = false; }
      bool running() const override { return alive; }
      int channels() const override { return alive ? (name == "サンプルASIO機器" ? 64 : 2) : 0; }
    };
    AppState a;
    a.loadDefaults();
    auto* system = new NamedOutput("サンプル音声機器");
    auto* asio = new NamedOutput("サンプルASIO機器");
    AudioController audio(a, std::unique_ptr<backhaul::AudioOutput>(system), std::unique_ptr<backhaul::AudioOutput>(asio));
    CHECK(audio.asioAvailable() && !a.audioAsio());
    CHECK(audio.deviceNames().value(1) == QStringLiteral("サンプル音声機器"));
    audio.chooseDevice(1);
    CHECK(system->alive && a.property("audioChannels").toInt() == 2);

    audio.setAsio(true);
    CHECK(a.audioAsio() && !system->alive && audio.deviceIndex() == 0);
    CHECK(a.property("audioDeviceName").toString().isEmpty() && !a.property("audioOk").toBool());
    CHECK(audio.deviceNames().value(1) == QStringLiteral("サンプルASIO機器"));
    audio.chooseDevice(1);
    CHECK(asio->alive && !system->alive);
    CHECK(a.property("audioChannels").toInt() == 32);  // the table shows at most 32 channels
    CHECK(a.property("audioDevice").toString() == QStringLiteral("サンプルASIO機器（ASIO）"));

    // Saved as ASIO: next time the ASIO device is started again...
    {
      AppState b;
      CHECK(b.applySettings(a.settingsJson()));
      auto* system2 = new NamedOutput("サンプル音声機器");
      auto* asio2 = new NamedOutput("サンプルASIO機器");
      AudioController again(b, std::unique_ptr<backhaul::AudioOutput>(system2), std::unique_ptr<backhaul::AudioOutput>(asio2));
      again.startSaved();
      CHECK(asio2->alive && !system2->alive && again.deviceIndex() == 1);
    }
    // ...and a program without ASIO says so instead of silently using something else.
    {
      AppState b;
      CHECK(b.applySettings(a.settingsJson()));
      auto* system2 = new NamedOutput("サンプル音声機器");
      AudioController again(b, std::unique_ptr<backhaul::AudioOutput>(system2));
      again.startSaved();
      CHECK(!system2->alive && !again.problem().isEmpty());
    }
    audio.setAsio(false);
    CHECK(!asio->alive && !a.audioAsio());
  }

#ifdef BACKHAUL_HAVE_ASIO
  {  // the real ASIO host code: listing drivers and refusing an unknown one must not crash
    auto real = backhaul::makeAsioOutput();
    CHECK(real != nullptr);
    if (real != nullptr) {
      const auto found = real->devices();
      std::printf("ASIO drivers on this PC: %d\n", static_cast<int>(found.size()));
      backhaul::AudioMixer silent(1);
      std::string error;
      CHECK(!real->start("no such driver", &silent, &error));
      CHECK(!error.empty() && !real->running() && real->channels() == 0);
    }
  }
#else
  CHECK(backhaul::makeAsioOutput() == nullptr);
#endif

#ifdef _WIN32
  {  // the real Windows sound devices can at least be listed without trouble
    auto real = backhaul::makeAudioOutput();
    CHECK(real != nullptr);
    if (real != nullptr) {
      const auto found = real->devices();
      std::printf("sound devices on this PC: %d\n", static_cast<int>(found.size()));
      for (const auto& d : found) CHECK(!d.id.empty() && !d.name.empty());
      CHECK(!real->running() && real->channels() == 0);
      backhaul::AudioMixer silent(1);
      std::string error;
      CHECK(!real->start("no-such-device", &silent, &error));
      CHECK(!error.empty() && !real->running());
      if (!found.empty()) {  // play 200 ms of silence on the first device
        if (real->start(found[0].id, &silent, &error)) {
          CHECK(real->running() && real->channels() >= 1);
          Sleep(200);
          real->stop();
          CHECK(!real->running());
        } else {
          std::printf("could not start the first device: %s\n", error.c_str());
        }
      }
    }
  }

  {  // the real Windows store, under a name of its own so saved credentials are not touched
    auto store = makeCredentialStore(QStringLiteral("backhaul/self-test"));
    CHECK(store != nullptr);
    if (store != nullptr) {
      CHECK(store->clear());
      ZoomCredentials got;
      CHECK(!store->load(got));
      CHECK(store->save({QStringLiteral("SampleClientId_01"), QStringLiteral("サンプルの秘密 sample-secret")}));
      CHECK(store->load(got));
      CHECK(got.clientId == QStringLiteral("SampleClientId_01"));
      CHECK(got.clientSecret == QStringLiteral("サンプルの秘密 sample-secret"));
      CHECK(store->clear());
      CHECK(!store->load(got));
      CHECK(store->clear());  // clearing twice is fine
    }
  }
#endif

  if (testing::failures() == 0) {
    std::printf("all meeting controller tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
