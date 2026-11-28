#include <QCoreApplication>
#include <QGuiApplication>
#include <algorithm>
#include <cmath>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

#include <QImage>
#include <QJsonArray>
#include <QJsonObject>

#include "app_state.h"
#include "audio_controller.h"
#include "check.h"
#include "frame_store.h"
#include "meeting_controller.h"
#include "return_controller.h"

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
  void setShareWant(std::uint32_t sourceId) override { shareWant = sourceId; }
  std::uint32_t shareWant = 0;
  bool sendAudio(const std::int16_t* samples, std::size_t frames, int channels) override {
    if (!taking) return false;
    sent.insert(sent.end(), samples, samples + frames * static_cast<std::size_t>(channels));
    sentChannels = channels;
    return true;
  }
  bool sendVideo(const std::uint8_t* i420, int width, int height) override {
    if (!taking || i420 == nullptr) return false;
    ++pictures;
    pictureWidth = width;
    pictureHeight = height;
    firstByte = i420[0];
    return true;
  }
  backhaul::LineQuality line;
  backhaul::LineQuality lineQuality() const override { return line; }
  bool sendingVideo() const override { return taking; }
  std::string sendVideoNote() const override { return note; }
  int pictures = 0, pictureWidth = 0, pictureHeight = 0, firstByte = -1;
  bool sendingAudio() const override { return taking; }
  std::string sendAudioNote() const override { return note; }

  bool taking = false;
  std::string note;
  std::vector<std::int16_t> sent;
  int sentChannels = 0;

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

// thrown out of the meeting by the line: this PC comes back by itself, and only then
static void testRejoin() {
  using backhaul::EndCause;
  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("all"));
  a.setZoomName(0, QStringLiteral("Kakuu Ichiro"));
  a.setMeetingId(QStringLiteral("123 4567 8901"));
  FrameStore frames;
  ZoomCredentials saved{QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")};
  auto* client = new ScriptedClient;
  MeetingController m(a, frames, std::unique_ptr<backhaul::MeetingClient>(client),
                      std::make_unique<MemoryCredentials>(&saved));
  qint64 now = 1'000'000;
  m.setClock([&now] { return now; });
  CHECK(a.rejoins());
  Participant self = person(1, "会場 PC");
  self.isSelf = true;
  // What the meeting library does when the meeting is over for this PC: everybody is gone,
  // then the reason, then the state.
  auto drop = [&](MeetingState state, EndCause cause) {
    client->events->onParticipants({});
    client->events->onEndCause(cause);
    client->events->onMeetingState(state, "会議に入れませんでした (2)");
  };
  auto in = [&] {
    client->events->onMeetingState(MeetingState::Receiving, {});
    client->events->onParticipants({self, person(10, "Kakuu Ichiro")});
  };

  m.join();
  in();
  CHECK(client->joins == 1 && a.property("connected").toBool() && slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));

  // The line drops: the first try is set for 5 seconds from now, and the operator's wish
  // to be in the meeting still stands (busy, no second "join", no update in between).
  drop(MeetingState::Failed, EndCause::Connection);
  CHECK(m.rejoining() && m.rejoinAttempt() == 1 && m.rejoinWaitSeconds() == 5);
  CHECK(m.busy() && !m.canJoin() && !m.failed() && !a.property("connected").toBool() && client->joins == 1);
  CHECK(m.statusText() == QStringLiteral("入り直し中"));
  // The slot stays the person's, shown as lost.
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("lost"));
  CHECK(slotData(a, 0, SlotModel::ZoomNameRole).toString() == QStringLiteral("Kakuu Ichiro"));
  now += 3000;
  CHECK(m.rejoinWaitSeconds() == 2);

  // The try (started here by hand, as "try now" does; the timer does the same).
  m.rejoinNow();
  CHECK(client->joins == 2 && m.rejoinWaitSeconds() == 0 && m.rejoining());
  CHECK(client->lastRequest.meetingNumber == 12345678901ULL);
  m.rejoinNow();  // not waiting: nothing happens
  CHECK(client->joins == 2);
  // Still no line: the next tries wait 10, 20, then 30 seconds.
  drop(MeetingState::Failed, EndCause::Connection);
  CHECK(m.rejoinAttempt() == 2 && m.rejoinWaitSeconds() == 10);
  m.rejoinNow();
  drop(MeetingState::Failed, EndCause::Connection);
  CHECK(m.rejoinAttempt() == 3 && m.rejoinWaitSeconds() == 20);
  m.rejoinNow();
  drop(MeetingState::Failed, EndCause::Connection);
  CHECK(m.rejoinAttempt() == 4 && m.rejoinWaitSeconds() == 30);
  // Back in: the person returns to the slot by name, and coming back is over.
  m.rejoinNow();
  CHECK(client->joins == 5);
  client->events->onMeetingState(MeetingState::Receiving, {});
  client->events->onParticipants({self, person(27, "Kakuu Ichiro")});  // a new number, the same name
  CHECK(!m.rejoining() && a.property("connected").toBool() && m.busy());
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));

  // The host ends the meeting, or takes this PC out: that is not undone.
  drop(MeetingState::Ended, EndCause::Host);
  CHECK(!m.rejoining() && !m.busy() && m.canJoin() && client->joins == 5);
  // A meeting that does not take this PC (a wrong passcode) is not tried again either,
  // also not in the middle of coming back.
  m.join();
  in();
  drop(MeetingState::Failed, EndCause::Connection);
  m.rejoinNow();
  drop(MeetingState::Failed, EndCause::Refused);
  CHECK(!m.rejoining() && m.failed() && !m.busy() && client->joins == 7);
  // A join that never got in is not "coming back": the first failure is simply shown.
  m.join();
  drop(MeetingState::Failed, EndCause::Connection);
  CHECK(!m.rejoining() && m.failed() && client->joins == 8);
  // Nothing said about the reason: left alone.
  m.join();
  in();
  drop(MeetingState::Ended, EndCause::None);
  CHECK(!m.rejoining() && !m.busy());

  // "Leave" (and "stop" on the strip) while waiting for a try ends it for good.
  m.join();
  in();
  const int leaves = client->leaves;
  drop(MeetingState::Failed, EndCause::Connection);
  CHECK(m.rejoining());
  m.leave();
  CHECK(!m.rejoining() && !m.busy() && m.canJoin() && !m.failed() && client->leaves == leaves);
  CHECK(m.statusText() == QStringLiteral("未接続"));
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
  // ...and while a try is under way, it leaves at the meeting service too.
  m.join();
  in();
  drop(MeetingState::Failed, EndCause::Connection);
  m.rejoinNow();
  m.leave();
  CHECK(!m.rejoining() && !m.busy() && client->leaves == leaves + 1);
  // Leaving by hand in the normal way is never followed by a try.
  m.join();
  in();
  m.leave();
  CHECK(!m.rejoining() && !m.busy());

  // Ten minutes without getting back in: trying stops, and the screen says so.
  m.join();
  in();
  const int before = client->joins;
  drop(MeetingState::Failed, EndCause::Connection);
  int tries = 0;
  while (m.rejoining() && tries < 100) {
    now += qint64{1000} * m.rejoinWaitSeconds();
    m.rejoinNow();
    ++tries;
    drop(MeetingState::Failed, EndCause::Connection);
  }
  CHECK(tries >= 20 && tries < 30 && client->joins == before + tries);
  CHECK(!m.rejoining() && m.failed() && !m.busy() && m.detail().contains(QStringLiteral("10分")));

  // Switched off in the settings: a dropped line is just the end.
  a.setRejoins(false);
  m.join();
  in();
  drop(MeetingState::Failed, EndCause::Connection);
  CHECK(!m.rejoining() && m.failed() && !m.busy());
  a.setRejoins(true);
}

// how good the line is: this PC's own, and each person's
static void testLine() {
  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("all"));
  a.setZoomName(0, QStringLiteral("Kakuu Ichiro"));
  a.setZoomName(1, QStringLiteral("Mihon Hanako"));
  a.setMeetingId(QStringLiteral("123 4567 8901"));
  FrameStore frames;
  ZoomCredentials saved{QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")};
  auto* client = new ScriptedClient;
  MeetingController m(a, frames, std::unique_ptr<backhaul::MeetingClient>(client),
                      std::make_unique<MemoryCredentials>(&saved));
  qint64 now = 1'000'000;
  m.setClock([&now] { return now; });
  Participant self = person(1, "会場 PC");
  self.isSelf = true;
  Participant ichiro = person(10, "Kakuu Ichiro"), hanako = person(11, "Mihon Hanako");

  // Not in a meeting: nothing is known, nothing is shown.
  client->line = {5, 0};
  m.refreshLine();
  CHECK(m.line().isEmpty());

  m.join();
  client->events->onMeetingState(MeetingState::Receiving, {});
  client->events->onParticipants({self, ichiro, hanako});
  m.refreshLine();
  CHECK(m.line() == QStringLiteral("good"));
  client->line = {3, 0};  // "not good" on the way in
  m.refreshLine();
  CHECK(m.line() == QStringLiteral("fair"));
  client->line = {5, 2};  // fine on the way in, bad on the way out: the worse one counts
  m.refreshLine();
  CHECK(m.line() == QStringLiteral("poor"));
  client->line = {0, 0};  // the meeting says nothing: nothing is claimed
  m.refreshLine();
  CHECK(m.line().isEmpty());

  // One person's line turns bad: unstable after two seconds, not at once, and only that person.
  ichiro.lineStep = 2;
  hanako.lineStep = 5;
  client->events->onParticipants({self, ichiro, hanako});
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  now += 1900;
  m.refreshLine();
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  now += 200;
  m.refreshLine();
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("unstable"));
  CHECK(slotData(a, 1, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  // "Not good" is not "bad": no mark for that.
  hanako.lineStep = 3;
  client->events->onParticipants({self, ichiro, hanako});
  now += 5000;
  m.refreshLine();
  CHECK(slotData(a, 1, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  // Better again: the mark goes at once.
  ichiro.lineStep = 4;
  client->events->onParticipants({self, ichiro, hanako});
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  // A short dip leaves no mark.
  ichiro.lineStep = 1;
  client->events->onParticipants({self, ichiro, hanako});
  now += 1000;
  ichiro.lineStep = 5;
  client->events->onParticipants({self, ichiro, hanako});
  now += 5000;
  m.refreshLine();
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));

  // Out of the meeting: nothing of it stays.
  ichiro.lineStep = 1;
  client->events->onParticipants({self, ichiro, hanako});
  now += 3000;
  m.refreshLine();
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("unstable"));
  client->line = {5, 5};
  m.refreshLine();
  CHECK(m.line() == QStringLiteral("good"));
  m.leave();
  CHECK(m.line().isEmpty() && slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
}

// devices that go away come back by themselves; what the operator switched off stays off
static void testDevicesComeBack() {
  struct Speaker final : backhaul::AudioOutput {
    bool alive = false, present = true, refuse = false;
    int starts = 0;
    std::vector<backhaul::AudioDevice> devices() override {
      return present ? std::vector<backhaul::AudioDevice>{{"spk", "サンプル卓出力"}} : std::vector<backhaul::AudioDevice>{};
    }
    bool start(const std::string&, backhaul::AudioMixer*, std::string* error) override {
      if (refuse) {
        *error = "not ready";
        return alive = false;
      }
      ++starts;
      return alive = true;
    }
    void stop() override { alive = false; }
    bool running() const override { return alive; }
    int channels() const override { return alive ? 8 : 0; }
  };
  struct Mic final : backhaul::AudioInput {
    bool alive = false, present = true;
    int starts = 0;
    std::vector<backhaul::AudioDevice> devices() override {
      return present ? std::vector<backhaul::AudioDevice>{{"mic-a", "サンプル卓入力A"}} : std::vector<backhaul::AudioDevice>{};
    }
    bool start(const std::string&, backhaul::AudioSink*, std::string*) override {
      ++starts;
      return alive = true;
    }
    void stop() override { alive = false; }
    bool running() const override { return alive; }
    int channels() const override { return alive ? 2 : 0; }
  };
  struct Capture final : backhaul::VideoInput {
    bool alive = false, present = true;
    int starts = 0;
    std::vector<backhaul::VideoDevice> devices() override {
      return present ? std::vector<backhaul::VideoDevice>{{"cap-a", "サンプルキャプチャーA"}} : std::vector<backhaul::VideoDevice>{};
    }
    bool start(const std::string&, backhaul::VideoSink*, std::string*) override {
      ++starts;
      return alive = true;
    }
    void stop() override { alive = false; }
    bool running() const override { return alive; }
    int width() const override { return alive ? 1920 : 0; }
    int height() const override { return alive ? 1080 : 0; }
    int milliFps() const override { return alive ? 59940 : 0; }
  };

  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("all"));
  a.setReturnInAll(true);  // this PC also sends sound and picture back
  CHECK(a.playsAudio() && a.sendsAudio() && a.sendsVideo());
  FrameStore frames;
  auto* speaker = new Speaker;
  auto* mic = new Mic;
  auto* capture = new Capture;
  AudioController audio(a, std::unique_ptr<backhaul::AudioOutput>(speaker), nullptr, std::unique_ptr<backhaul::AudioInput>(mic));
  audio.pumpByHand();
  ReturnController returning(a, frames, std::unique_ptr<backhaul::VideoInput>(capture));

  // Nothing chosen: nothing is looked for.
  audio.watchDevices(true);
  returning.watchDevice(true);
  CHECK(!audio.outputSeeking() && !audio.micSeeking() && !returning.seeking());

  audio.chooseDevice(1);
  audio.chooseMic(1);
  returning.chooseDevice(1);
  CHECK(audio.deviceIndex() == 1 && audio.micIndex() == 1 && returning.deviceIndex() == 1 && a.property("audioOk").toBool());
  CHECK(!audio.outputSeeking() && !audio.micSeeking() && !returning.seeking());

  // The output device is pulled out: noticed, said, and looked for.
  speaker->alive = false;
  speaker->present = false;
  audio.watchDevices(true);
  CHECK(audio.deviceIndex() == 0 && !a.property("audioOk").toBool() && audio.outputSeeking() && audio.problem().contains(QStringLiteral("探しています")));
  CHECK(a.property("audioDeviceName").toString() == QStringLiteral("サンプル卓出力"));  // the choice is kept
  audio.watchDevices(true);
  CHECK(audio.outputSeeking() && speaker->starts == 1);
  // Plugged in again but not ready yet: the next look tries again, and the message stays.
  speaker->present = true;
  speaker->refuse = true;
  audio.watchDevices(true);
  CHECK(audio.outputSeeking() && audio.problem().contains(QStringLiteral("探しています")));
  speaker->refuse = false;
  audio.watchDevices(true);
  CHECK(!audio.outputSeeking() && audio.deviceIndex() == 1 && a.property("audioOk").toBool() && audio.problem().isEmpty() && speaker->starts == 2);

  // Looking keeps its pace: without "now", a second look right away does nothing.
  speaker->alive = false;
  audio.watchDevices(true);   // notices, and looks: the device is there, so it is back at once
  CHECK(audio.deviceIndex() == 1 && speaker->starts == 3);
  speaker->alive = false;
  audio.watchDevices();       // notices; the last look was a moment ago
  CHECK(audio.outputSeeking() && speaker->starts == 3);
  audio.watchDevices(true);
  CHECK(!audio.outputSeeking() && speaker->starts == 4);

  // The same for the recording device...
  mic->alive = false;
  mic->present = false;
  audio.watchDevices(true);
  CHECK(audio.micIndex() == 0 && audio.micSeeking() && audio.micProblem().contains(QStringLiteral("探しています")));
  mic->present = true;
  audio.watchDevices(true);
  CHECK(audio.micIndex() == 1 && !audio.micSeeking() && audio.micProblem().isEmpty() && mic->starts == 2);
  // ...and for the capture device (also when only its signal changed: it stops and starts again).
  capture->alive = false;
  returning.watchDevice(true);
  CHECK(returning.deviceIndex() == 1 && !returning.seeking() && capture->starts == 2);
  capture->alive = false;
  capture->present = false;
  returning.watchDevice(true);
  CHECK(returning.deviceIndex() == 0 && returning.seeking() && returning.problem().contains(QStringLiteral("探しています")));
  capture->present = true;
  returning.watchDevice(true);
  CHECK(returning.deviceIndex() == 1 && !returning.seeking() && returning.problem().isEmpty() && capture->starts == 3);

  // What the operator switches off is not brought back.
  audio.chooseDevice(0);
  audio.chooseMic(0);
  returning.chooseDevice(0);
  audio.watchDevices(true);
  returning.watchDevice(true);
  CHECK(!audio.outputSeeking() && !audio.micSeeking() && !returning.seeking());
  CHECK(audio.deviceIndex() == 0 && audio.micIndex() == 0 && returning.deviceIndex() == 0 && speaker->starts == 4);

  // A PC whose role leaves sound and picture to others looks for nothing, whatever is saved.
  audio.chooseDevice(1);
  returning.chooseDevice(1);
  a.setRole(QStringLiteral("camera"));
  audio.watchDevices(true);
  returning.watchDevice(true);
  CHECK(!audio.outputSeeking() && !audio.micSeeking() && !returning.seeking() && !speaker->alive && !capture->alive);
}

// a spare sound device takes over when the main one stays away
static void testSpareDevice() {
  struct Rack final : backhaul::AudioOutput {
    bool mainThere = true, spareThere = true;
    std::string on;
    std::vector<std::string> started;
    std::vector<backhaul::AudioDevice> devices() override {
      std::vector<backhaul::AudioDevice> list;
      if (mainThere) list.push_back({"main", "サンプル卓出力"});
      if (spareThere) list.push_back({"spare", "サンプル予備出力"});
      list.push_back({"other", "サンプル別出力"});
      return list;
    }
    bool start(const std::string& id, backhaul::AudioMixer*, std::string*) override {
      started.push_back(id);
      on = id;
      return true;
    }
    void stop() override { on.clear(); }
    bool running() const override { return !on.empty(); }
    int channels() const override { return on.empty() ? 0 : 8; }
  };
  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("all"));
  auto* rack = new Rack;
  AudioController audio(a, std::unique_ptr<backhaul::AudioOutput>(rack));
  qint64 now = 1'000'000;
  audio.setClock([&now] { return now; });

  // No main device: no spare to choose from either.
  CHECK(audio.spareNames() == (QStringList{QStringLiteral("なし"), QStringLiteral("サンプル卓出力"), QStringLiteral("サンプル予備出力"), QStringLiteral("サンプル別出力")}));
  audio.chooseDevice(1);
  // The main device is not offered as its own spare.
  CHECK(audio.spareNames() == (QStringList{QStringLiteral("なし"), QStringLiteral("サンプル予備出力"), QStringLiteral("サンプル別出力")}));
  CHECK(audio.spareIndex() == 0 && !audio.onSpare());

  // Without a spare, a lost main device is only looked for.
  rack->on.clear();
  rack->mainThere = false;
  audio.watchDevices(true);
  now += 10000;
  audio.watchDevices(true);
  CHECK(audio.outputSeeking() && !audio.onSpare() && rack->started.size() == 1);
  rack->mainThere = true;
  audio.watchDevices(true);
  CHECK(audio.deviceIndex() > 0 && rack->on == "main");

  // With a spare: a quick re-plug is no reason to switch...
  audio.chooseSpare(1);
  CHECK(a.spareDeviceName() == QStringLiteral("サンプル予備出力") && audio.spareIndex() == 1 && audio.spareThere());
  rack->on.clear();
  rack->mainThere = false;
  audio.watchDevices(true);
  now += 2900;
  audio.watchDevices(true);
  CHECK(audio.outputSeeking() && !audio.onSpare() && rack->on.empty());
  rack->mainThere = true;
  audio.watchDevices(true);
  CHECK(rack->on == "main" && !audio.onSpare());
  // ...three seconds without the main device are.
  rack->on.clear();
  rack->mainThere = false;
  audio.watchDevices(true);
  now += 3000;
  audio.watchDevices(true);
  CHECK(rack->on == "spare" && audio.onSpare() && !audio.outputSeeking() && a.property("audioOk").toBool());
  CHECK(a.property("audioDeviceName").toString() == QStringLiteral("サンプル卓出力"));  // the main device stays the main device
  CHECK(audio.problem().contains(QStringLiteral("予備")) && !audio.mainBack());

  // The main device is back: said, but not switched to (that would interrupt the sound again).
  rack->mainThere = true;
  audio.watchDevices(true);
  CHECK(audio.mainBack() && audio.onSpare() && rack->on == "spare");
  audio.returnToMain();
  CHECK(rack->on == "main" && !audio.onSpare() && !audio.mainBack() && audio.problem().isEmpty());

  // Going back although the main device is not there: looked for, and the spare again after three seconds.
  rack->on.clear();
  rack->mainThere = false;
  audio.watchDevices(true);
  now += 3000;
  audio.watchDevices(true);
  CHECK(audio.onSpare());
  audio.returnToMain();
  CHECK(!audio.onSpare() && audio.outputSeeking() && rack->on.empty());
  now += 3000;
  audio.watchDevices(true);
  CHECK(audio.onSpare() && rack->on == "spare");

  // The spare goes too: both are looked for, and the main one wins when both are there.
  rack->on.clear();
  rack->spareThere = false;
  audio.watchDevices(true);
  CHECK(!audio.onSpare() && audio.outputSeeking());
  CHECK(audio.spareNames().contains(QStringLiteral("サンプル予備出力")) && !audio.spareThere());  // the choice stays visible
  now += 5000;
  rack->mainThere = rack->spareThere = true;
  audio.watchDevices(true);
  CHECK(rack->on == "main" && !audio.onSpare());

  // Choosing the spare as the main device leaves no spare; so does "no output".
  audio.chooseDevice(2);
  CHECK(a.property("audioDeviceName").toString() == QStringLiteral("サンプル予備出力") && a.spareDeviceName().isEmpty());
  audio.chooseSpare(1);
  CHECK(a.spareDeviceName() == QStringLiteral("サンプル卓出力"));
  audio.chooseDevice(0);
  CHECK(a.spareDeviceName().isEmpty() && !audio.outputSeeking());

  // Kept in the settings, and a file that names the main device as its own spare is put right.
  audio.chooseDevice(1);
  audio.chooseSpare(1);
  AppState b;
  CHECK(b.applySettings(a.settingsJson()) && b.spareDeviceName() == QStringLiteral("サンプル予備出力"));
  QJsonObject odd = a.settingsJson();
  QJsonObject sound = odd.value(QStringLiteral("audio")).toObject();
  sound[QStringLiteral("spare")] = sound.value(QStringLiteral("device"));
  odd[QStringLiteral("audio")] = sound;
  AppState c;
  CHECK(c.applySettings(odd) && c.spareDeviceName().isEmpty());
}

// a PC that shows one person: one slot, the largest picture, and no sound at all
static void testCameraRole() {
  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("camera"));
  a.setZoomName(0, QStringLiteral("Kakuu Ichiro"));
  a.setZoomName(1, QStringLiteral("Mihon Hanako"));
  a.setMeetingId(QStringLiteral("123 4567 8901"));
  FrameStore frames;
  ZoomCredentials saved{QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")};
  auto* client = new ScriptedClient;
  MeetingController m(a, frames, std::unique_ptr<backhaul::MeetingClient>(client),
                      std::make_unique<MemoryCredentials>(&saved));
  backhaul::AudioMixer mixer(4);
  mixer.setChannels(0, 1);
  m.setMixer(&mixer);
  m.join();
  CHECK(client->joins == 1 && !client->lastRequest.audio);
  client->events->onMeetingState(MeetingState::Receiving, {});
  Participant self = person(1, "会場 カメラ1");
  self.isSelf = true;
  client->events->onParticipants({self, person(10, "Kakuu Ichiro"), person(11, "Mihon Hanako")});
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));
  CHECK(slotData(a, 1, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));  // not this PC's job
  CHECK(client->wants.size() == 1 && client->wants[0].userId == 10 && client->wants[0].size == VideoSize::P1080);
  a.setQualityAuto(false);
  CHECK(client->wants.size() == 1 && client->wants[0].size == VideoSize::P720);
  a.setQualityAuto(true);

  // Sound that arrives anyway is dropped.
  std::vector<std::int16_t> loud(480, 20000);
  client->events->onAudio(10, loud.data(), loud.size(), 48000, 1);
  std::vector<float> out(480, 0.0f);
  mixer.render(out.data(), 480, 1);
  CHECK(mixer.takePeak(0) == 0.0f);

  // Someone else is put on: the first person leaves the picture.
  a.assignTo(0, 2);
  CHECK(client->wants.size() == 1 && client->wants[0].userId == 11);
  a.assignTo(1, 1);  // slot 2 is not in use in this role
  CHECK(slotData(a, 1, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
  m.leave();
}

// two people side by side: two slots at 720p; the do-everything role joins with sound
static void testTwoShotRole() {
  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("twoshot"));
  a.setZoomName(0, QStringLiteral("Kakuu Ichiro"));
  a.setZoomName(1, QStringLiteral("Mihon Hanako"));
  a.setZoomName(2, QStringLiteral("Shiken Goro"));
  a.setMeetingId(QStringLiteral("123 4567 8901"));
  FrameStore frames;
  ZoomCredentials saved{QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")};
  auto* client = new ScriptedClient;
  MeetingController m(a, frames, std::unique_ptr<backhaul::MeetingClient>(client),
                      std::make_unique<MemoryCredentials>(&saved));
  m.join();
  CHECK(!client->lastRequest.audio);
  client->events->onMeetingState(MeetingState::Receiving, {});
  client->events->onParticipants({person(10, "Kakuu Ichiro"), person(11, "Mihon Hanako", false), person(12, "Shiken Goro")});
  CHECK(client->wants.size() == 2);
  CHECK(client->wants[0].userId == 10 && client->wants[0].size == VideoSize::P720);
  CHECK(client->wants[1].userId == 11 && client->wants[1].size == VideoSize::P720);
  CHECK(slotData(a, 2, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
  // The second person's camera is off: with "the other one full screen" only the left half is drawn.
  CHECK(a.twoShotSplit() == 0);
  a.setTwoShotSolo(true);
  CHECK(a.twoShotSplit() == 1);
  client->events->onParticipants({person(10, "Kakuu Ichiro"), person(11, "Mihon Hanako", true), person(12, "Shiken Goro")});
  CHECK(a.twoShotSplit() == 0);
  m.leave();

  a.setRole(QStringLiteral("all"));
  m.join();
  CHECK(client->joins == 2 && client->lastRequest.audio);
  client->events->onMeetingState(MeetingState::Receiving, {});
  client->events->onParticipants({person(10, "Kakuu Ichiro"), person(11, "Mihon Hanako"), person(12, "Shiken Goro")});
  CHECK(client->wants.size() == 3);  // every slot is in use again
  m.leave();
}

// the sound PC: no pictures, every person can be heard, and a device's sound goes into the meeting
static void testAudioRole() {
  struct FakeSpeaker final : backhaul::AudioOutput {
    bool alive = false;
    int inputs = 0;
    backhaul::AudioSink* sink = nullptr;
    std::vector<backhaul::AudioDevice> devices() override { return {{"spk", "サンプル卓出力"}}; }
    bool start(const std::string&, backhaul::AudioMixer*, std::string*) override { return alive = true; }
    void stop() override { alive = false; }
    bool running() const override { return alive; }
    int channels() const override { return alive ? 8 : 0; }
    int inputChannels() const override { return alive ? inputs : 0; }
    void setInputSink(backhaul::AudioSink* s) override { sink = s; }
  };
  struct FakeMic final : backhaul::AudioInput {
    bool alive = false, refuse = false;
    backhaul::AudioSink* sink = nullptr;
    std::vector<backhaul::AudioDevice> devices() override { return {{"mic-a", "サンプル卓入力A"}, {"mic-b", "サンプル卓入力B"}}; }
    bool start(const std::string&, backhaul::AudioSink* s, std::string* error) override {
      if (refuse) {
        *error = "busy";
        return alive = false;
      }
      sink = s;
      return alive = true;
    }
    void stop() override { alive = false; }
    bool running() const override { return alive; }
    int channels() const override { return alive ? 2 : 0; }
  };

  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("audio"));
  CHECK(a.handlesAudio() && a.sendsAudio() && !a.receivesVideo());
  a.setZoomName(0, QStringLiteral("Kakuu Ichiro"));
  a.setMeetingId(QStringLiteral("123 4567 8901"));
  FrameStore frames;
  ZoomCredentials saved{QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")};
  auto* client = new ScriptedClient;
  MeetingController m(a, frames, std::unique_ptr<backhaul::MeetingClient>(client),
                      std::make_unique<MemoryCredentials>(&saved));
  auto* speaker = new FakeSpeaker;
  auto* asio = new FakeSpeaker;
  asio->inputs = 5;
  auto* mic = new FakeMic;
  {
    AudioController audio(a, std::unique_ptr<backhaul::AudioOutput>(speaker), std::unique_ptr<backhaul::AudioOutput>(asio),
                          std::unique_ptr<backhaul::AudioInput>(mic));
    // No sending thread in this test: blocks are handed over by calling pumpOnce().
    audio.pumpByHand();
    std::vector<std::int16_t> got;
    int gotChannels = 0;
    audio.setSender([&](const std::int16_t* samples, std::size_t n, int channels) {
      got.insert(got.end(), samples, samples + n * static_cast<std::size_t>(channels));
      gotChannels = channels;
      return true;
    });
    m.setMixer(audio.mixer());

    // Without a recording device the PC joins with sound but sends none.
    m.join();
    CHECK(client->lastRequest.audio && !client->lastRequest.sendAudio);
    m.leave();

    // Sending settings. Stereo needs "original" or "music".
    a.setSendStereo(true);
    CHECK(!a.sendStereo());
    a.setSendQuality(7);
    CHECK(a.sendQuality() == 0);
    a.setSendQuality(2);
    CHECK(a.sendQuality() == 2 && a.sendStereo());

    // A Windows recording device.
    CHECK(audio.micAvailable());
    CHECK(audio.micNames() == (QStringList{QStringLiteral("使わない"), QStringLiteral("サンプル卓入力A"),
                                           QStringLiteral("サンプル卓入力B")}));
    CHECK(audio.micIndex() == 0);
    audio.chooseMic(9);  // not in the list
    CHECK(audio.micIndex() == 0 && a.micDeviceName().isEmpty());
    mic->refuse = true;
    audio.chooseMic(2);
    CHECK(audio.micIndex() == 0 && !audio.micProblem().isEmpty());
    CHECK(a.micDeviceName() == QStringLiteral("サンプル卓入力B"));  // the wish is kept
    mic->refuse = false;
    audio.chooseMic(2);
    CHECK(mic->alive && audio.micIndex() == 2 && audio.micProblem().isEmpty());
    CHECK(audio.micNames().value(2) == QStringLiteral("サンプル卓入力B"));

    // What the device records reaches the sender in 10 ms blocks, both sides kept apart.
    std::vector<float> block(1000 * 2);
    for (std::size_t i = 0; i < 1000; ++i) {
      block[i * 2] = 0.5f;
      block[i * 2 + 1] = -0.25f;
    }
    mic->sink->onCaptured(block.data(), 1000, 2);
    CHECK(audio.pumpOnce() == 2);            // 960 of the 1000 frames
    CHECK(got.size() == 960 * 2 && gotChannels == 2);
    CHECK(got[0] == 16384 && got[1] == -8192);
    CHECK(audio.pumpOnce() == 0);            // the rest waits for a full block
    a.setSendQuality(0);                     // speech: one side, the average of both
    got.clear();
    mic->sink->onCaptured(block.data(), 1000, 2);
    CHECK(audio.pumpOnce() == 2);
    CHECK(got.size() == 960 && gotChannels == 1 && got[0] == 4096);
    a.setSendQuality(2);

    // Joining now asks the meeting to take the sound, with the chosen quality.
    m.join();
    CHECK(client->lastRequest.audio && client->lastRequest.sendAudio);
    CHECK(client->lastRequest.sendQuality == backhaul::SendQuality::Music && client->lastRequest.sendStereo);
    client->events->onMeetingState(MeetingState::Receiving, {});
    client->events->onParticipants({person(10, "Kakuu Ichiro"), person(11, "Mihon Hanako")});
    CHECK(client->wants.empty());  // a sound-only PC asks for no pictures
    CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("ok"));
    std::vector<std::int16_t> voice(480, 12000);
    a.toggleRoute(0, 1);  // defaults already send slot 1 to channel 1: off...
    a.toggleRoute(0, 1);  // ...and on again
    client->events->onAudio(10, voice.data(), voice.size(), 48000, 1);
    CHECK(audio.mixer()->takePeak(0) > 0.3f);  // voices are heard on this PC
    client->taking = true;
    CHECK(m.sendAudio(voice.data(), 240, 2));
    CHECK(client->sent.size() == 480 && client->sentChannels == 2);
    m.leave();

    // A role that sends nothing stops the recording device and refuses to start one.
    a.setRole(QStringLiteral("all"));
    CHECK(!mic->alive && audio.micIndex() == 0);
    audio.chooseMic(1);
    CHECK(!mic->alive && !audio.micProblem().isEmpty());
    a.setRole(QStringLiteral("audio"));
    CHECK(mic->alive && audio.micIndex() == 1);  // chooseMic(1) was remembered as the wish

    // The return PC sends too, but plays nothing: the recording device runs, the speaker does not.
    audio.chooseDevice(1);
    CHECK(speaker->alive);
    a.setRole(QStringLiteral("camera"));
    CHECK(!mic->alive && !speaker->alive);
    a.setRole(QStringLiteral("return"));
    CHECK(mic->alive && audio.micIndex() == 1 && !speaker->alive);
    a.setRole(QStringLiteral("audio"));
    CHECK(mic->alive && speaker->alive);

    // ASIO: the inputs are pairs of the running output device.
    audio.setAsio(true);
    CHECK(!mic->alive && audio.micIndex() == 0 && a.micDeviceName().isEmpty());
    CHECK(audio.micNames().size() == 1);
    audio.chooseDevice(1);
    CHECK(asio->alive);
    CHECK(audio.micNames() == (QStringList{QStringLiteral("使わない"), QStringLiteral("入力 1-2"), QStringLiteral("入力 3-4"),
                                           QStringLiteral("入力 5-6")}));
    audio.chooseMic(2);
    CHECK(audio.micIndex() == 2 && asio->sink != nullptr && a.micDeviceName() == QStringLiteral("入力 3-4"));
    std::vector<float> five(480 * 5, 0.0f);
    for (std::size_t i = 0; i < 480; ++i) {
      five[i * 5 + 2] = 0.5f;   // input 3
      five[i * 5 + 3] = 0.25f;  // input 4
    }
    got.clear();
    asio->sink->onCaptured(five.data(), 480, 5);
    CHECK(audio.pumpOnce() == 1);
    CHECK(got.size() == 960 && got[958] == 16384 && got[959] == 8192);  // the chosen pair, left and right
    audio.chooseDevice(0);  // the output device is switched off: its inputs go with it
    CHECK(asio->sink == nullptr && audio.micIndex() == 0);
    CHECK(a.micDeviceName() == QStringLiteral("入力 3-4"));
    audio.chooseDevice(1);  // and come back with it
    CHECK(audio.micIndex() == 2 && asio->sink != nullptr);

    // The sending choices survive a restart; older files simply have none.
    AppState b;
    CHECK(b.applySettings(a.settingsJson()));
    CHECK(b.micDeviceName() == QStringLiteral("入力 3-4") && b.sendQuality() == 2 && b.sendStereo());
    QJsonObject old = a.settingsJson();
    QJsonObject oldAudio = old.value(QStringLiteral("audio")).toObject();
    oldAudio.remove(QStringLiteral("input"));
    oldAudio.remove(QStringLiteral("sendQuality"));
    oldAudio.remove(QStringLiteral("sendStereo"));
    old[QStringLiteral("audio")] = oldAudio;
    CHECK(b.applySettings(old));
    CHECK(b.micDeviceName().isEmpty() && b.sendQuality() == 0 && !b.sendStereo());
    oldAudio[QStringLiteral("input")] = QStringLiteral("x");
    oldAudio[QStringLiteral("sendQuality")] = QStringLiteral("loud");
    oldAudio[QStringLiteral("sendStereo")] = false;
    old[QStringLiteral("audio")] = oldAudio;
    CHECK(!b.applySettings(old));
  }
}

// the shared screen: shown in the slide slot, held or blacked when sharing stops
static void testSlides() {
  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("slide"));
  a.setMeetingId(QStringLiteral("123 4567 8901"));
  const int row = a.shareRow();
  FrameStore frames;
  ZoomCredentials saved{QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")};
  auto* client = new ScriptedClient;
  MeetingController m(a, frames, std::unique_ptr<backhaul::MeetingClient>(client),
                      std::make_unique<MemoryCredentials>(&saved));
  m.join();
  CHECK(!client->lastRequest.audio);
  client->events->onMeetingState(MeetingState::Receiving, {});
  client->events->onParticipants({person(10, "Kakuu Ichiro"), person(11, "Mihon Hanako")});
  CHECK(client->wants.empty());  // nobody's camera on a slide PC
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));
  CHECK(slotData(a, row, SlotModel::VideoOutRole).toString() == QStringLiteral("black"));
  CHECK(client->shareWant == 0);

  // Someone starts sharing: that screen is asked for and shown.
  client->events->onShares({{501, 10}});
  CHECK(client->shareWant == 501);
  CHECK(slotData(a, row, SlotModel::VideoOutRole).toString() == QStringLiteral("live"));
  CHECK(slotData(a, row, SlotModel::ZoomNameRole).toString() == QStringLiteral("共有：Kakuu Ichiro"));
  sendFrame(client->events, 10, 640, 360);       // a camera picture: not for this PC
  CHECK(frames.latest(row).isNull());
  client->events->onShareFrame(777, backhaul::VideoFrame{});  // another screen: ignored
  {
    const std::vector<std::uint8_t> y(1920 * 1200, 126), c(960 * 600, 128);
    backhaul::VideoFrame f;
    f.y = y.data();
    f.u = c.data();
    f.v = c.data();
    f.width = 1920;
    f.height = 1200;
    client->events->onShareFrame(777, f);
    CHECK(frames.latest(row).isNull());
    client->events->onShareFrame(501, f);
  }
  CHECK(frames.latest(row).width() == 1920 && frames.latest(row).height() == 1200);

  // A second person shares as well: the screen already showing stays.
  client->events->onShares({{502, 11}, {501, 10}});
  CHECK(client->shareWant == 501);
  // The first one stops: the other screen takes over by itself.
  client->events->onShares({{502, 11}});
  CHECK(client->shareWant == 502);
  CHECK(slotData(a, row, SlotModel::ZoomNameRole).toString() == QStringLiteral("共有：Mihon Hanako"));

  // Sharing stops altogether: hold the last picture, or black, as chosen.
  frames.put(row, QImage(16, 9, QImage::Format_RGB32));
  a.slotList()->update(row, [](SlotRow& r) { r.situation.hasLastFrame = true; });
  a.setFreezeOnLoss(row, true);
  client->events->onShares({});
  CHECK(client->shareWant == 0);
  CHECK(slotData(a, row, SlotModel::VideoOutRole).toString() == QStringLiteral("freeze"));
  a.setFreezeOnLoss(row, false);
  CHECK(slotData(a, row, SlotModel::VideoOutRole).toString() == QStringLiteral("black"));
  client->events->onShares({{503, 10}});
  CHECK(client->shareWant == 503 && slotData(a, row, SlotModel::VideoOutRole).toString() == QStringLiteral("live"));
  // Who shares is not a setting: it is not saved, and it is gone when the meeting ends.
  CHECK(!slotData(a, row, SlotModel::ZoomNameRole).toString().isEmpty());
  CHECK(a.settingsJson().value(QStringLiteral("slots")).toArray().at(row).toObject().value(QStringLiteral("zoomName")).toString().isEmpty());

  // A PC that shows one person takes no shared screen; the do-everything PC does.
  m.leave();
  a.setRole(QStringLiteral("camera"));
  m.join();
  client->events->onMeetingState(MeetingState::Receiving, {});
  client->events->onShares({{601, 10}});
  CHECK(client->shareWant == 0);
  CHECK(slotData(a, row, SlotModel::VideoOutRole).toString() == QStringLiteral("black"));
  m.leave();
  a.setRole(QStringLiteral("all"));
  m.join();
  client->events->onMeetingState(MeetingState::Receiving, {});
  client->events->onShares({{601, 10}});
  CHECK(client->shareWant == 601);
  m.leave();
  CHECK(slotData(a, row, SlotModel::VideoOutRole).toString() == QStringLiteral("black"));
  CHECK(slotData(a, row, SlotModel::ZoomNameRole).toString().isEmpty());
}

// the return PC: a capture device's picture goes into the meeting, as camera or as shared screen
static void testReturnRole() {
  struct FakeCapture final : backhaul::VideoInput {
    bool alive = false, refuse = false;
    backhaul::VideoSink* sink = nullptr;
    std::vector<backhaul::VideoDevice> devices() override { return {{"cap-a", "サンプルキャプチャーA"}, {"cap-b", "サンプルキャプチャーB"}}; }
    bool start(const std::string&, backhaul::VideoSink* s, std::string* error) override {
      if (refuse) {
        *error = "in use";
        return alive = false;
      }
      sink = s;
      return alive = true;
    }
    void stop() override { alive = false; }
    bool running() const override { return alive; }
    int width() const override { return alive ? 1920 : 0; }
    int height() const override { return alive ? 1080 : 0; }
    int milliFps() const override { return alive ? 59940 : 0; }
  };

  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("return"));
  CHECK(a.sendsVideo() && a.sendsAudio() && a.handlesAudio() && !a.playsAudio() && !a.receivesAnything());
  CHECK(!a.slotActive(0) && !a.slotActive(4));
  a.setMeetingId(QStringLiteral("123 4567 8901"));
  FrameStore frames;
  ZoomCredentials saved{QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")};
  auto* client = new ScriptedClient;
  MeetingController m(a, frames, std::unique_ptr<backhaul::MeetingClient>(client),
                      std::make_unique<MemoryCredentials>(&saved));
  auto* capture = new FakeCapture;
  ReturnController returning(a, frames, std::unique_ptr<backhaul::VideoInput>(capture));
  returning.setSender([&m](const std::uint8_t* i420, int w, int h) { return m.sendVideo(i420, w, h); });

  // Nothing chosen: the PC joins but sends no picture, and asks for nothing from the meeting.
  m.join();
  CHECK(client->lastRequest.sendVideo == backhaul::SendVideo::None && !client->lastRequest.receive);
  // With no sound device chosen it stays out of the meeting's sound altogether
  // (joining it would open this PC's own microphone).
  CHECK(!client->lastRequest.audio && !client->lastRequest.sendAudio);
  a.setMicDeviceName(QStringLiteral("サンプル卓入力A"));
  m.leave();
  m.join();
  CHECK(client->lastRequest.audio && client->lastRequest.sendAudio);
  a.setMicDeviceName(QString());
  m.leave();

  CHECK(returning.available());
  CHECK(returning.deviceNames() == (QStringList{QStringLiteral("使わない"), QStringLiteral("サンプルキャプチャーA"),
                                                QStringLiteral("サンプルキャプチャーB")}));
  capture->refuse = true;
  returning.chooseDevice(2);
  CHECK(returning.deviceIndex() == 0 && !returning.problem().isEmpty());
  CHECK(a.videoDeviceName() == QStringLiteral("サンプルキャプチャーB"));  // the wish is kept
  capture->refuse = false;
  returning.chooseDevice(2);
  CHECK(capture->alive && returning.deviceIndex() == 2 && returning.problem().isEmpty());
  CHECK(returning.inputText() == QStringLiteral("1920×1080・59.94コマ"));
  returning.chooseDevice(5);  // not in the list
  CHECK(returning.deviceIndex() == 2);

  // As this PC's camera, or as a shared screen.
  m.join();
  CHECK(client->lastRequest.sendVideo == backhaul::SendVideo::Camera);
  client->events->onMeetingState(MeetingState::Receiving, {});
  client->events->onParticipants({person(10, "Kakuu Ichiro")});
  CHECK(client->wants.empty());
  CHECK(slotData(a, 0, SlotModel::StateRole).toString() == QStringLiteral("unassigned"));  // nobody is shown here
  client->events->onShares({{501, 10}});
  CHECK(client->shareWant == 0);  // and no shared screen is taken

  // Pictures from the device: a small NV12 picture with padded rows, at 59.94 per second.
  const int w = 4, h = 2, stride = 8;
  std::vector<std::uint8_t> y(static_cast<std::size_t>(stride * h), 200), uv(static_cast<std::size_t>(stride), 128);
  y[0] = 77;
  client->taking = false;
  capture->sink->onFrame(y.data(), stride, uv.data(), stride, w, h, 0);
  CHECK(client->pictures == 0);                       // the meeting is not taking pictures yet
  CHECK(frames.latest(ReturnController::kPreviewSlot).width() == 4);  // but the operator sees the input
  client->taking = true;
  for (int i = 1; i <= 120; ++i) capture->sink->onFrame(y.data(), stride, uv.data(), stride, w, h, i * 16683LL);
  CHECK(client->pictures >= 58 && client->pictures <= 62);  // two seconds of input, 30 per second out
  CHECK(client->pictureWidth == 4 && client->pictureHeight == 2 && client->firstByte == 77);

  // "Stop sending" keeps the PC in the meeting and the pictures here.
  const int before = client->pictures;
  returning.setPaused(true);
  for (int i = 121; i <= 180; ++i) capture->sink->onFrame(y.data(), stride, uv.data(), stride, w, h, i * 16683LL);
  CHECK(client->pictures == before);
  returning.setPaused(false);
  for (int i = 181; i <= 240; ++i) capture->sink->onFrame(y.data(), stride, uv.data(), stride, w, h, i * 16683LL);
  CHECK(client->pictures > before + 25);
  // A broken picture is dropped, not sent.
  const int sane = client->pictures;
  capture->sink->onFrame(y.data(), 2, uv.data(), stride, w, h, 99999999LL);
  CHECK(client->pictures == sane);
  m.leave();

  a.setReturnAsShare(true);
  m.join();
  CHECK(client->lastRequest.sendVideo == backhaul::SendVideo::Share && !client->lastRequest.receive);
  // A shared screen is not thinned out: 59.94 pictures per second in, the same out.
  client->events->onMeetingState(MeetingState::Receiving, {});
  const int shareStart = client->pictures;
  for (int i = 0; i < 120; ++i) capture->sink->onFrame(y.data(), stride, uv.data(), stride, w, h, 200000000LL + i * 16683LL);
  CHECK(client->pictures - shareStart >= 118);
  // A faster source is held to 60.
  const int fastStart = client->pictures;
  for (int i = 0; i < 240; ++i) capture->sink->onFrame(y.data(), stride, uv.data(), stride, w, h, 300000000LL + i * 8333LL);
  CHECK(client->pictures - fastStart >= 118 && client->pictures - fastStart <= 124);
  m.leave();

  // Another job: the capture device is let go, and comes back with the role.
  a.setRole(QStringLiteral("camera"));
  CHECK(!capture->alive && returning.deviceIndex() == 0);
  CHECK(frames.latest(ReturnController::kPreviewSlot).isNull());
  returning.chooseDevice(1);
  CHECK(!capture->alive && !returning.problem().isEmpty());
  m.join();
  CHECK(client->lastRequest.sendVideo == backhaul::SendVideo::None && client->lastRequest.receive);
  m.leave();
  a.setRole(QStringLiteral("return"));
  CHECK(capture->alive && returning.deviceIndex() == 1);  // chooseDevice(1) was remembered as the wish

  // The choices survive a restart; older files have none; damaged ones are refused.
  AppState b;
  CHECK(b.applySettings(a.settingsJson()));
  CHECK(b.role() == QStringLiteral("return") && b.returnAsShare() && b.videoDeviceName() == QStringLiteral("サンプルキャプチャーA"));
  QJsonObject old = a.settingsJson();
  old.remove(QStringLiteral("return"));
  CHECK(b.applySettings(old));
  CHECK(!b.returnAsShare() && b.videoDeviceName().isEmpty());
  old[QStringLiteral("return")] = QJsonObject{{QStringLiteral("device"), QStringLiteral("x")}, {QStringLiteral("as"), QStringLiteral("window")}};
  CHECK(!b.applySettings(old));

  // ---- the all-in-one PC sends the return too, but only when its owner says so ----
  {
    AppState all;
    all.loadDefaults();
    all.setRole(QStringLiteral("all"));
    all.setMeetingId(QStringLiteral("123 4567 8901"));
    // Devices remembered from an earlier job on this PC.
    all.setMicDeviceName(QStringLiteral("サンプル卓入力A"));
    FrameStore allFrames;
    ZoomCredentials keys{QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")};
    auto* allClient = new ScriptedClient;
    MeetingController meeting(all, allFrames, std::unique_ptr<backhaul::MeetingClient>(allClient),
                              std::make_unique<MemoryCredentials>(&keys));
    auto* device = new FakeCapture;
    ReturnController back(all, allFrames, std::unique_ptr<backhaul::VideoInput>(device));
    back.setSender([&meeting](const std::uint8_t* i420, int w, int h) { return meeting.sendVideo(i420, w, h); });

    // Off by default: remembered devices send nothing, and the capture device is left alone.
    CHECK(!all.returnInAll() && !all.sendsVideo() && !all.sendsAudio() && all.playsAudio() && all.receivesAnything());
    back.chooseDevice(1);
    CHECK(!device->alive && back.deviceIndex() == 0);
    meeting.join();
    CHECK(allClient->lastRequest.sendVideo == backhaul::SendVideo::None && !allClient->lastRequest.sendAudio);
    CHECK(allClient->lastRequest.receive && allClient->lastRequest.audio);
    meeting.leave();

    // Switched on: it sends the picture and the sound, and still receives everything.
    int roleChanges = 0;
    QObject::connect(&all, &AppState::roleChanged, &all, [&roleChanges] { ++roleChanges; });
    all.setReturnInAll(true);
    all.setReturnInAll(true);
    CHECK(roleChanges == 1);
    CHECK(all.sendsVideo() && all.sendsAudio() && all.playsAudio() && all.receivesVideo() && all.receivesAnything());
    back.chooseDevice(1);
    CHECK(device->alive && back.deviceIndex() == 1);
    meeting.join();
    CHECK(allClient->lastRequest.sendVideo == backhaul::SendVideo::Camera && allClient->lastRequest.sendAudio);
    CHECK(allClient->lastRequest.receive && allClient->lastRequest.audio);
    meeting.leave();

    // Switched off again: the capture device is let go at once.
    all.setReturnInAll(false);
    CHECK(!device->alive && back.deviceIndex() == 0 && !all.sendsVideo());

    // The switch means nothing to the other roles, and is kept in the settings.
    all.setReturnInAll(true);
    for (const char* role : {"camera", "twoshot", "slide"}) {
      all.setRole(QString::fromLatin1(role));
      CHECK(!all.sendsVideo() && !all.sendsAudio());
    }
    all.setRole(QStringLiteral("audio"));
    CHECK(!all.sendsVideo() && all.sendsAudio());
    all.setRole(QStringLiteral("all"));
    AppState kept;
    CHECK(kept.applySettings(all.settingsJson()));
    CHECK(kept.returnInAll() && kept.sendsVideo());
    all.setReturnInAll(false);
    CHECK(kept.applySettings(all.settingsJson()));
    CHECK(!kept.returnInAll());
    QJsonObject damaged = all.settingsJson();
    QJsonObject part = damaged.value(QStringLiteral("return")).toObject();
    part[QStringLiteral("inAll")] = QStringLiteral("yes");
    damaged[QStringLiteral("return")] = part;
    CHECK(!kept.applySettings(damaged));
    kept.setReturnInAll(true);
    kept.loadDefaults();
    CHECK(!kept.returnInAll());
  }
}

// sound check: a test signal on chosen outputs, which never reaches the meeting and never lingers
static void testSoundCheck() {
  struct CheckOutput final : backhaul::AudioOutput {
    bool alive = false;
    std::vector<backhaul::AudioDevice> devices() override { return {{"desk-a", "サンプル卓出力A"}, {"desk-b", "サンプル卓出力B"}}; }
    bool start(const std::string&, backhaul::AudioMixer*, std::string*) override { return alive = true; }
    void stop() override { alive = false; }
    bool running() const override { return alive; }
    int channels() const override { return alive ? 4 : 0; }
  };
  struct CheckMic final : backhaul::AudioInput {
    bool alive = false;
    backhaul::AudioSink* sink = nullptr;
    std::vector<backhaul::AudioDevice> devices() override { return {{"mic-a", "サンプル卓入力A"}}; }
    bool start(const std::string&, backhaul::AudioSink* s, std::string*) override {
      sink = s;
      return alive = true;
    }
    void stop() override { alive = false; }
    bool running() const override { return alive; }
    int channels() const override { return alive ? 2 : 0; }
  };
  auto loudest = [](const std::vector<float>& block, std::size_t channel) {
    float peak = 0.0f;
    for (std::size_t i = 240; i * 4 + channel < block.size(); ++i) peak = std::max(peak, std::fabs(block[i * 4 + channel]));
    return peak;
  };

  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("audio"));
  auto* out = new CheckOutput;
  auto* mic = new CheckMic;
  AudioController audio(a, std::unique_ptr<backhaul::AudioOutput>(out), nullptr, std::unique_ptr<backhaul::AudioInput>(mic));
  audio.pumpByHand();  // blocks are handed over by calling pumpOnce() below
  std::vector<std::int16_t> sent;
  audio.setSender([&](const std::int16_t* samples, std::size_t n, int channels) {
    sent.insert(sent.end(), samples, samples + n * static_cast<std::size_t>(channels));
    return true;
  });
  int notes = 0;
  QObject::connect(&audio, &AudioController::testChanged, [&] { ++notes; });

  // Without a running device there is nothing to check.
  CHECK(audio.testOn().isEmpty() && !audio.testing());
  audio.toggleTest(0);
  audio.sweepTest();
  CHECK(!audio.testing() && !audio.testSweeping() && notes == 0);

  audio.chooseDevice(1);
  audio.chooseMic(1);
  CHECK(audio.testOn().size() == 4 && audio.testLevelDb() == -20 && audio.testKind() == 0);

  // Channels switch on and off one by one; the signal is on exactly those.
  audio.toggleTest(1);
  audio.toggleTest(3);
  audio.toggleTest(4);   // no such channel
  audio.toggleTest(-1);
  CHECK(audio.testing() && audio.testChannelsText() == QStringLiteral("2, 4"));
  CHECK(!audio.testOn()[0].toBool() && audio.testOn()[1].toBool() && !audio.testOn()[2].toBool() && audio.testOn()[3].toBool());
  std::vector<float> block(480 * 4);
  audio.mixer()->render(block.data(), 480, 4);
  CHECK_NEAR(loudest(block, 1), 0.1, 0.003);
  CHECK_NEAR(loudest(block, 3), 0.1, 0.003);
  CHECK_NEAR(loudest(block, 0), 0.0, 1e-9);
  CHECK_NEAR(loudest(block, 2), 0.0, 1e-9);

  // What goes to the meeting is what the recording device hears, nothing of the test signal.
  const std::vector<float> quiet(480 * 2, 0.0f);
  mic->sink->onCaptured(quiet.data(), 480, 2);
  CHECK(audio.pumpOnce() == 1 && !sent.empty());
  CHECK(std::all_of(sent.begin(), sent.end(), [](std::int16_t v) { return v == 0; }));

  audio.toggleTest(1);
  CHECK(audio.testChannelsText() == QStringLiteral("4"));

  // The level moves in steps of 2 dB and stops at both ends, well below full level.
  for (int i = 0; i < 40; ++i) audio.nudgeTestLevel(1);
  CHECK(audio.testLevelDb() == -6 && audio.testLevelMax() == -6);
  for (int i = 0; i < 40; ++i) audio.nudgeTestLevel(-1);
  CHECK(audio.testLevelDb() == -40 && audio.testLevelMin() == -40);
  audio.setTestKind(1);
  audio.setTestKind(7);  // unknown: ignored
  CHECK(audio.testKind() == 1);

  // The sweep goes through every channel once and then falls silent.
  audio.sweepTest();
  CHECK(audio.testSweeping() && audio.testChannelsText() == QStringLiteral("1"));
  audio.sweepStep();
  CHECK(audio.testChannelsText() == QStringLiteral("2"));
  audio.sweepStep();
  audio.sweepStep();
  CHECK(audio.testSweeping() && audio.testChannelsText() == QStringLiteral("4"));
  audio.sweepStep();
  CHECK(!audio.testSweeping() && !audio.testing());
  audio.sweepStep();  // nothing running: nothing happens
  CHECK(!audio.testing());

  // A hand on a button during the sweep takes over.
  audio.sweepTest();
  audio.toggleTest(2);
  CHECK(!audio.testSweeping() && audio.testChannelsText() == QStringLiteral("3"));
  audio.stopTest();
  CHECK(!audio.testing());
  for (int i = 0; i < 4; ++i) audio.mixer()->render(block.data(), 480, 4);  // faded out
  CHECK_NEAR(loudest(block, 2), 0.0, 1e-9);

  // It never lingers: another device, no device, the device dying, another role.
  audio.toggleTest(0);
  audio.chooseDevice(2);
  CHECK(!audio.testing());
  audio.toggleTest(0);
  audio.chooseDevice(0);
  CHECK(!audio.testing() && audio.testOn().isEmpty());
  audio.chooseDevice(1);
  audio.sweepTest();
  a.setRole(QStringLiteral("camera"));
  CHECK(!audio.testing() && !audio.testSweeping());
  CHECK(audio.mixer()->tone().channels() == 0u);
}

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
    app.setRole(QStringLiteral("all"));  // this PC does everything
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
  app.setRole(QStringLiteral("all"));  // this PC does everything
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
    a.setRole(QStringLiteral("all"));  // this PC does everything
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
    a.setRole(QStringLiteral("all"));  // this PC does everything
    auto* system = new NamedOutput("サンプル音声機器");
    auto* asio = new NamedOutput("サンプルASIO機器");
    AudioController audio(a, std::unique_ptr<backhaul::AudioOutput>(system), std::unique_ptr<backhaul::AudioOutput>(asio));
    CHECK(audio.asioAvailable() && !a.audioAsio());
    CHECK(audio.deviceNames().value(1) == QStringLiteral("サンプル音声機器"));
    audio.chooseDevice(1);
    CHECK(system->alive && a.property("audioChannels").toInt() == 2);

    // A role that leaves the sound to another PC stops the device and refuses to start one;
    // back in the do-everything role the saved device plays again.
    a.setRole(QStringLiteral("camera"));
    CHECK(!system->alive && audio.deviceIndex() == 0 && !a.property("audioOk").toBool());
    CHECK(a.property("audioDeviceName").toString() == QStringLiteral("サンプル音声機器"));  // the choice is kept
    audio.chooseDevice(1);
    CHECK(!system->alive && !audio.problem().isEmpty());
    a.setRole(QStringLiteral("all"));
    CHECK(system->alive && audio.deviceIndex() == 1 && a.property("audioOk").toBool());

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


  testRejoin();
  testLine();
  testDevicesComeBack();
  testSpareDevice();
  testCameraRole();
  testTwoShotRole();
  testAudioRole();
  testSlides();
  testReturnRole();
  testSoundCheck();

  if (testing::failures() == 0) {
    std::printf("all meeting controller tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
