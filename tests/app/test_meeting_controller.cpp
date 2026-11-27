#include <QCoreApplication>
#include <QGuiApplication>
#include <vector>

#include "app_state.h"
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

#ifdef _WIN32
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
