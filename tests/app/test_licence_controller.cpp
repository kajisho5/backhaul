// The licence and the Zoom sign-in, with a made-up server (the real one is tested in server/).
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <deque>
#include <map>
#include <vector>

#include "app_state.h"
#include "check.h"
#include "frame_store.h"
#include "gate_client.h"
#include "licence_controller.h"
#include "meeting_controller.h"

namespace {

using backhaul::MeetingState;

// Answers what the test lined up, later (when flush() is called), like a network would.
struct FakeGate final : GateClient {
  struct Asked {
    QString path, bearer;
    QJsonObject body;
  };
  std::vector<Asked> asked;
  std::map<QString, std::deque<GateReply>> answers;
  std::vector<std::pair<QString, Done>> waiting;

  void post(const QString& path, const QJsonObject& body, const QString& bearer, Done done) override {
    asked.push_back({path, bearer, body});
    waiting.emplace_back(path, std::move(done));
  }
  void answer(const QString& path, int status, const QJsonObject& body) {
    GateReply r;
    r.status = status;
    r.body = body;
    answers[path].push_back(r);
  }
  void unreachable(const QString& path) {
    GateReply r;
    r.transport = QStringLiteral("Host not found");
    answers[path].push_back(r);
  }
  // Delivers every waiting answer, including those to requests made while delivering.
  void flush() {
    while (!waiting.empty()) {
      auto [path, done] = std::move(waiting.front());
      waiting.erase(waiting.begin());
      GateReply reply;
      reply.status = 500;
      auto& lined = answers[path];
      if (!lined.empty()) {
        reply = lined.front();
        lined.pop_front();
      }
      done(reply);
    }
  }
  int count(const QString& path) const {
    int n = 0;
    for (const Asked& a : asked) n += a.path == path ? 1 : 0;
    return n;
  }
  const Asked& last(const QString& path) const {
    for (auto it = asked.rbegin(); it != asked.rend(); ++it) {
      if (it->path == path) return *it;
    }
    static const Asked none;
    return none;
  }
};

struct MemorySecret final : SecretStore {
  QByteArray* shared;
  bool refuse = false;
  explicit MemorySecret(QByteArray* s) : shared(s) {}
  QByteArray load() override { return *shared; }
  bool save(const QByteArray& secret) override {
    if (refuse) return false;
    *shared = secret;
    return true;
  }
  bool clear() override {
    shared->clear();
    return true;
  }
};

QJsonObject licenceJson(int used = 1, double expiresAt = 1830000000.0) {
  return QJsonObject{{QStringLiteral("keyTail"), QStringLiteral("7Q2M")}, {QStringLiteral("seats"), 5},
                     {QStringLiteral("used"), used}, {QStringLiteral("expiresAt"), expiresAt},
                     {QStringLiteral("deviceName"), QStringLiteral("SAMPLE-PC-02")}};
}

struct Rig {
  QByteArray licenceSaved;
  FakeGate* gate = new FakeGate;
  MemorySecret* licenceStore = nullptr;
  QList<QUrl> opened;
  bool browserWorks = true;
  qint64 now = 1800000000;
  std::unique_ptr<LicenceController> licence;
  Rig() { make(); }
  // A fresh controller over the same saved secrets, as after a restart.
  void make() {
    gate = new FakeGate;
    licenceStore = new MemorySecret(&licenceSaved);
    licence = std::make_unique<LicenceController>(
        std::unique_ptr<GateClient>(gate), std::unique_ptr<SecretStore>(licenceStore), QStringLiteral("  SAMPLE-PC-02  "),
        [this](const QUrl& url) {
          opened.push_back(url);
          return browserWorks;
        },
        [this] { return now; });
  }
  void activate() {
    gate->answer(QStringLiteral("/v1/activate"), 200,
                 {{QStringLiteral("deviceToken"), QStringLiteral("sample-device-token")}, {QStringLiteral("licence"), licenceJson()}});
    licence->activate(QStringLiteral(" bh-aaaaa-bbbbb-ccccc-d7q2m "));
    gate->flush();
  }
  void signIn() {
    gate->answer(QStringLiteral("/v1/zoom/begin"), 200,
                 {{QStringLiteral("state"), QStringLiteral("sample-state")},
                  {QStringLiteral("url"), QStringLiteral("https://zoom.us/oauth/authorize?state=sample-state")}});
    licence->signIn();
    gate->flush();
    gate->answer(QStringLiteral("/v1/zoom/collect"), 200, {{QStringLiteral("signedIn"), true}});
    licence->pollSignIn();
    gate->flush();
  }
};

void testLicence() {
  {  // nowhere to keep secrets, or no server: switched off, and nothing is asked
    LicenceController off(nullptr, nullptr, QStringLiteral("x"), {});
    CHECK(!off.available() && !off.active());
    off.activate(QStringLiteral("BH-AAAAA-BBBBB-CCCCC-DDDDD"));
    off.signIn();
    bool called = false;
    off.prepareJoin([&](const LicenceController::Entry& e) {
      called = true;
      CHECK(!e.ok && !e.error.isEmpty());
    });
    CHECK(called);
  }

  Rig rig;
  LicenceController& l = *rig.licence;
  CHECK(l.available() && !l.active() && !l.busy() && l.expiresText().isEmpty());

  // A key the server does not know, a full licence, no network: each said in plain words.
  rig.gate->answer(QStringLiteral("/v1/activate"), 404, {{QStringLiteral("error"), QStringLiteral("unknown_key")}});
  l.activate(QStringLiteral("BH-AAAAA-BBBBB-CCCCC-DDDDD"));
  CHECK(l.busy());
  l.activate(QStringLiteral("BH-AAAAA-BBBBB-CCCCC-DDDDD"));  // a second press while waiting does nothing
  CHECK(rig.gate->count(QStringLiteral("/v1/activate")) == 1);
  rig.gate->flush();
  CHECK(!l.busy() && !l.active() && l.messageBad() && l.message().contains(QStringLiteral("見つかりません")));
  rig.gate->answer(QStringLiteral("/v1/activate"), 409,
                   {{QStringLiteral("error"), QStringLiteral("seats_full")}, {QStringLiteral("seats"), 2},
                    {QStringLiteral("devices"), QJsonArray{QStringLiteral("SAMPLE-PC-01"), QStringLiteral("SAMPLE-PC-03")}}});
  l.activate(QStringLiteral("BH-AAAAA-BBBBB-CCCCC-DDDDD"));
  rig.gate->flush();
  CHECK(l.message().contains(QStringLiteral("2台")) && l.message().contains(QStringLiteral("SAMPLE-PC-01、SAMPLE-PC-03")));
  rig.gate->unreachable(QStringLiteral("/v1/activate"));
  l.activate(QStringLiteral("BH-AAAAA-BBBBB-CCCCC-DDDDD"));
  rig.gate->flush();
  CHECK(!l.active() && l.message().contains(QStringLiteral("届きません")) && rig.licenceSaved.isEmpty());
  l.activate(QStringLiteral("   "));
  CHECK(rig.gate->count(QStringLiteral("/v1/activate")) == 3);

  // A good key: what was typed goes out trimmed, with this PC's name and a number it made up.
  rig.activate();
  const FakeGate::Asked& sent = rig.gate->last(QStringLiteral("/v1/activate"));
  CHECK(sent.body.value(QStringLiteral("key")).toString() == QStringLiteral("bh-aaaaa-bbbbb-ccccc-d7q2m"));
  CHECK(sent.body.value(QStringLiteral("deviceName")).toString() == QStringLiteral("SAMPLE-PC-02"));
  const QString deviceId = sent.body.value(QStringLiteral("deviceId")).toString();
  CHECK(deviceId.size() == 36);
  CHECK(l.active() && !l.messageBad() && l.message().isEmpty());
  CHECK(l.keyTail() == QStringLiteral("7Q2M") && l.seats() == 5 && l.used() == 1 && l.deviceName() == QStringLiteral("SAMPLE-PC-02"));
  CHECK(l.expiresText().endsWith(QStringLiteral(" まで")) && l.expiresText().startsWith(QStringLiteral("2027-12-")));
  // Kept in the protected store: the PC's token, never the key.
  CHECK(rig.licenceSaved.contains("sample-device-token") && !rig.licenceSaved.toLower().contains("aaaaa"));

  // After a restart it is there without asking; asking then brings the current numbers.
  rig.make();
  LicenceController& again = *rig.licence;
  CHECK(again.active() && again.keyTail() == QStringLiteral("7Q2M") && again.used() == 1);
  rig.gate->answer(QStringLiteral("/v1/status"), 200,
                   {{QStringLiteral("problem"), QJsonValue::Null}, {QStringLiteral("licence"), licenceJson(3, 0.0)}});
  again.refresh();
  rig.gate->flush();
  CHECK(rig.gate->last(QStringLiteral("/v1/status")).bearer == QStringLiteral("sample-device-token"));
  CHECK(again.used() == 3 && again.expiresText() == QStringLiteral("期限なし") && again.message().isEmpty());
  rig.gate->answer(QStringLiteral("/v1/status"), 200,
                   {{QStringLiteral("problem"), QStringLiteral("revoked")}, {QStringLiteral("licence"), licenceJson()}});
  again.refresh();
  rig.gate->flush();
  CHECK(again.active() && again.messageBad() && again.message().contains(QStringLiteral("止められています")));

  // Entering the key again keeps this PC's number, so it takes no second place.
  rig.activate();
  CHECK(rig.gate->last(QStringLiteral("/v1/activate")).body.value(QStringLiteral("deviceId")).toString() == deviceId);

  // Taking the PC out: only once the server said so.
  rig.gate->unreachable(QStringLiteral("/v1/deactivate"));
  again.deactivate();
  rig.gate->flush();
  CHECK(again.active() && again.message().contains(QStringLiteral("登録されたまま")));
  rig.gate->answer(QStringLiteral("/v1/deactivate"), 200, {{QStringLiteral("ok"), true}});
  again.deactivate();
  rig.gate->flush();
  CHECK(!again.active() && rig.licenceSaved.isEmpty() && !again.messageBad());

  // The server no longer knows this PC (taken out from elsewhere): the PC forgets too.
  rig.activate();
  rig.gate->answer(QStringLiteral("/v1/status"), 401, {{QStringLiteral("error"), QStringLiteral("unknown_device")}});
  again.refresh();
  rig.gate->flush();
  CHECK(!again.active() && rig.licenceSaved.isEmpty() && again.message().contains(QStringLiteral("入れ直して")));
}

void testJoinTokens() {
  Rig rig;
  LicenceController& l = *rig.licence;
  rig.activate();
  std::vector<LicenceController::Entry> got;
  auto take = [&](const LicenceController::Entry& e) { got.push_back(e); };
  const QString path = QStringLiteral("/v1/sdk-token");

  rig.gate->answer(path, 200, {{QStringLiteral("token"), QStringLiteral("sample.sdk.token")}, {QStringLiteral("expiresAt"), 1800086400.0}});
  l.prepareJoin(take);
  CHECK(got.empty());  // the answer comes later
  rig.gate->flush();
  CHECK(got.size() == 1 && got[0].ok && got[0].sdkToken == QStringLiteral("sample.sdk.token") && got[0].zak.isEmpty() && got[0].notice.isEmpty());

  // Used again for the rest of the day without asking...
  rig.now += 20 * 3600;
  l.prepareJoin(take);
  rig.gate->flush();
  CHECK(got.size() == 2 && got[1].ok && rig.gate->count(path) == 1);
  // ...a new one is asked for before it runs out...
  rig.now += 3 * 3600 + 30 * 60;
  rig.gate->unreachable(path);
  l.prepareJoin(take);
  rig.gate->flush();
  // ...and with the server out of reach the old one still serves while it is good.
  CHECK(got.size() == 3 && got[2].ok && got[2].sdkToken == QStringLiteral("sample.sdk.token") && rig.gate->count(path) == 2);
  rig.now += 3600;  // now it has run out
  rig.gate->unreachable(path);
  l.prepareJoin(take);
  rig.gate->flush();
  CHECK(got.size() == 4 && !got[3].ok && got[3].error.contains(QStringLiteral("届きません")));

  // A stopped licence, and a server without Zoom keys.
  rig.gate->answer(path, 403, {{QStringLiteral("error"), QStringLiteral("revoked")}});
  l.prepareJoin(take);
  rig.gate->flush();
  CHECK(!got[4].ok && got[4].error.contains(QStringLiteral("止められています")));
  rig.gate->answer(path, 403, {{QStringLiteral("error"), QStringLiteral("expired")}, {QStringLiteral("expiresAt"), 1800000000.0}});
  l.prepareJoin(take);
  rig.gate->flush();
  CHECK(!got[5].ok && got[5].error.contains(QStringLiteral("期限が切れています（20")));
  rig.gate->answer(path, 503, {{QStringLiteral("error"), QStringLiteral("not_configured")}});
  l.prepareJoin(take);
  rig.gate->flush();
  CHECK(!got[6].ok && got[6].error.contains(QStringLiteral("準備がまだ")));
}

void testZoomSignIn() {
  Rig rig;
  LicenceController& l = *rig.licence;
  const QString begin = QStringLiteral("/v1/zoom/begin"), collect = QStringLiteral("/v1/zoom/collect"), zakPath = QStringLiteral("/v1/zoom/zak");

  l.signIn();  // not without a licence
  CHECK(rig.gate->asked.empty() && l.zoomMessageBad());
  rig.activate();

  // The server may only ever send the browser to Zoom itself.
  rig.gate->answer(begin, 200, {{QStringLiteral("state"), QStringLiteral("s")}, {QStringLiteral("url"), QStringLiteral("https://zoom.us.example.invalid/x")}});
  l.signIn();
  rig.gate->flush();
  CHECK(rig.opened.isEmpty() && !l.zoomWaiting() && l.zoomMessageBad());
  rig.gate->answer(begin, 200, {{QStringLiteral("state"), QStringLiteral("s")}, {QStringLiteral("url"), QStringLiteral("http://zoom.us/oauth/authorize")}});
  l.signIn();
  rig.gate->flush();
  CHECK(rig.opened.isEmpty() && !l.zoomWaiting());
  rig.browserWorks = false;
  rig.gate->answer(begin, 200, {{QStringLiteral("state"), QStringLiteral("s")}, {QStringLiteral("url"), QStringLiteral("https://zoom.us/oauth/authorize")}});
  l.signIn();
  rig.gate->flush();
  CHECK(rig.opened.size() == 1 && !l.zoomWaiting() && l.zoomMessage().contains(QStringLiteral("ブラウザを開けません")));
  rig.browserWorks = true;

  // The usual way: browser opens, the PC asks until the answer is there.
  rig.gate->answer(begin, 200, {{QStringLiteral("state"), QStringLiteral("sample-state")},
                                {QStringLiteral("url"), QStringLiteral("https://zoom.us/oauth/authorize?state=sample-state")}});
  l.signIn();
  rig.gate->flush();
  const QString challenge = rig.gate->last(begin).body.value(QStringLiteral("challenge")).toString();
  CHECK(challenge.size() == 43 && rig.opened.size() == 2 && l.zoomWaiting() && !l.zoomSignedIn() && !l.zoomMessageBad());
  l.signIn();  // already waiting
  CHECK(rig.gate->count(begin) == 4);
  rig.gate->answer(collect, 202, {{QStringLiteral("pending"), true}});
  l.pollSignIn();
  rig.gate->flush();
  rig.gate->unreachable(collect);  // a hiccup just waits for the next round
  l.pollSignIn();
  rig.gate->flush();
  CHECK(l.zoomWaiting());
  rig.gate->answer(collect, 200, {{QStringLiteral("signedIn"), true}});
  l.pollSignIn();
  rig.gate->flush();
  const FakeGate::Asked& collected = rig.gate->last(collect);
  CHECK(collected.body.value(QStringLiteral("state")).toString() == QStringLiteral("sample-state"));
  // The verifier matches the challenge that went out first (PKCE), and it never went out before.
  const QString verifier = collected.body.value(QStringLiteral("verifier")).toString();
  CHECK(verifier.size() == 64 && !rig.gate->last(begin).body.contains(QStringLiteral("verifier")));
  CHECK(l.zoomSignedIn() && !l.zoomWaiting() && l.zoomMessage().isEmpty());
  // Nothing of the sign-in itself is on this PC: only that there is one.
  CHECK(rig.licenceSaved.contains("\"zoomSignedIn\":true") && !rig.licenceSaved.contains("refresh"));

  // Joining now brings the ZAK; the PC sends nothing but its own token for it.
  rig.gate->answer(QStringLiteral("/v1/sdk-token"), 200, {{QStringLiteral("token"), QStringLiteral("sample.sdk.token")}, {QStringLiteral("expiresAt"), 1800086400.0}});
  rig.gate->answer(zakPath, 200, {{QStringLiteral("zak"), QStringLiteral("sample-zak")}});
  LicenceController::Entry entry;
  l.prepareJoin([&](const LicenceController::Entry& e) { entry = e; });
  rig.gate->flush();
  CHECK(entry.ok && entry.zak == QStringLiteral("sample-zak") && entry.notice.isEmpty());
  CHECK(rig.gate->last(zakPath).body.isEmpty() && rig.gate->last(zakPath).bearer == QStringLiteral("sample-device-token"));

  // No ZAK this time (Zoom is down): the join goes ahead and says so; the sign-in stays.
  rig.gate->answer(zakPath, 502, {{QStringLiteral("error"), QStringLiteral("zoom_failed")}, {QStringLiteral("detail"), QStringLiteral("Zoom did not give a ZAK (503)")}});
  l.prepareJoin([&](const LicenceController::Entry& e) { entry = e; });
  rig.gate->flush();
  CHECK(entry.ok && entry.zak.isEmpty() && entry.notice.contains(QStringLiteral("503")) && l.zoomSignedIn());

  // Another PC of the licence (here: after a restart) learns of the sign-in from the server.
  rig.licenceSaved.replace("\"zoomSignedIn\":true", "\"zoomSignedIn\":false");
  rig.make();
  CHECK(!rig.licence->zoomSignedIn());
  QJsonObject withSignIn = licenceJson();
  withSignIn[QStringLiteral("zoomSignedIn")] = true;
  rig.gate->answer(QStringLiteral("/v1/status"), 200, {{QStringLiteral("problem"), QJsonValue::Null}, {QStringLiteral("licence"), withSignIn}});
  rig.licence->refresh();
  rig.gate->flush();
  CHECK(rig.licence->zoomSignedIn());
  // The sign-in is no longer good at Zoom: said, and no longer shown as signed in.
  rig.gate->answer(QStringLiteral("/v1/sdk-token"), 200, {{QStringLiteral("token"), QStringLiteral("t")}, {QStringLiteral("expiresAt"), 1800086400.0}});
  rig.gate->answer(zakPath, 401, {{QStringLiteral("error"), QStringLiteral("signed_out")}});
  rig.licence->prepareJoin([&](const LicenceController::Entry& e) { entry = e; });
  rig.gate->flush();
  CHECK(entry.ok && entry.zak.isEmpty() && entry.notice.contains(QStringLiteral("サインインが切れました")));
  CHECK(!rig.licence->zoomSignedIn());
  // Not signed in: Zoom is not asked for a ZAK at all.
  const int zakAsked = rig.gate->count(zakPath);
  rig.licence->prepareJoin([&](const LicenceController::Entry& e) { entry = e; });
  rig.gate->flush();
  CHECK(entry.ok && entry.notice.isEmpty() && rig.gate->count(zakPath) == zakAsked);

  // "No" in the browser, running out of time, giving up, and signing out.
  LicenceController& m = *rig.licence;
  auto startAgain = [&] {
    rig.gate->answer(begin, 200, {{QStringLiteral("state"), QStringLiteral("s2")}, {QStringLiteral("url"), QStringLiteral("https://zoom.us/oauth/authorize")}});
    m.signIn();
    rig.gate->flush();
    CHECK(m.zoomWaiting());
  };
  startAgain();
  rig.gate->answer(collect, 410, {{QStringLiteral("error"), QStringLiteral("denied")}});
  m.pollSignIn();
  rig.gate->flush();
  CHECK(!m.zoomWaiting() && !m.zoomSignedIn() && m.zoomMessage().contains(QStringLiteral("許可されなかった")));
  startAgain();
  rig.now += 601;
  m.pollSignIn();
  CHECK(!m.zoomWaiting() && m.zoomMessage().contains(QStringLiteral("時間内")) && rig.gate->count(collect) == 1);
  startAgain();
  m.cancelSignIn();
  CHECK(!m.zoomWaiting() && m.zoomMessage().isEmpty());
  rig.signIn();
  CHECK(m.zoomSignedIn());
  // Signing out counts once the server said so (it is for every PC of the licence).
  rig.gate->unreachable(QStringLiteral("/v1/zoom/signout"));
  m.signOut();
  rig.gate->flush();
  CHECK(m.zoomSignedIn() && m.zoomMessage().contains(QStringLiteral("そのまま")));
  rig.gate->answer(QStringLiteral("/v1/zoom/signout"), 200, {{QStringLiteral("revoked"), false}});
  m.signOut();
  rig.gate->flush();
  CHECK(!m.zoomSignedIn() && m.zoomMessage().contains(QStringLiteral("確認できませんでした")));
  rig.signIn();
  rig.gate->answer(QStringLiteral("/v1/zoom/signout"), 200, {{QStringLiteral("revoked"), true}});
  m.signOut();
  rig.gate->flush();
  CHECK(!m.zoomSignedIn() && m.zoomMessage().contains(QStringLiteral("PCすべて")));
}

// A meeting client that only notes what it was asked to join with.
struct NotingClient final : backhaul::MeetingClient {
  backhaul::JoinRequest last;
  int joins = 0;
  backhaul::MeetingEvents* events = nullptr;
  bool join(const backhaul::JoinRequest& request, backhaul::MeetingEvents* sink) override {
    last = request;
    events = sink;
    ++joins;
    sink->onMeetingState(MeetingState::Authenticating, {});
    return true;
  }
  void leave() override {
    if (events != nullptr) events->onMeetingState(MeetingState::Idle, {});
  }
  void setVideoWants(const std::vector<backhaul::VideoWant>&) override {}
  void poll() override {}
  void setShareWant(std::uint32_t) override {}
  bool sendAudio(const std::int16_t*, std::size_t, int) override { return false; }
  bool sendVideo(const std::uint8_t*, int, int) override { return false; }
  bool sendingVideo() const override { return false; }
  std::string sendVideoNote() const override { return {}; }
  bool sendingAudio() const override { return false; }
  std::string sendAudioNote() const override { return {}; }
};

struct KeptCredentials final : CredentialStore {
  ZoomCredentials kept;
  bool load(ZoomCredentials& out) override {
    out = kept;
    return !kept.clientId.isEmpty();
  }
  bool save(const ZoomCredentials& c) override {
    kept = c;
    return true;
  }
  bool clear() override {
    kept = {};
    return true;
  }
};

void testJoining() {
  Rig rig;
  AppState a;
  a.loadDefaults();
  a.setRole(QStringLiteral("all"));
  a.setMeetingId(QStringLiteral("123 4567 8901"));
  FrameStore frames;
  auto* client = new NotingClient;
  MeetingController m(a, frames, std::unique_ptr<backhaul::MeetingClient>(client), std::make_unique<KeptCredentials>());
  CHECK(!m.canJoin());  // no Zoom keys, no licence
  m.setLicence(rig.licence.get());
  CHECK(!m.canJoin());
  rig.activate();
  CHECK(m.canJoin());
  rig.signIn();

  // The join waits for the server, then starts with the token and the ZAK from there.
  rig.gate->answer(QStringLiteral("/v1/sdk-token"), 200, {{QStringLiteral("token"), QStringLiteral("sample.sdk.token")}, {QStringLiteral("expiresAt"), 1800086400.0}});
  rig.gate->answer(QStringLiteral("/v1/zoom/zak"), 200, {{QStringLiteral("zak"), QStringLiteral("sample-zak")}});
  m.setProperty("zak", QStringLiteral("typed-by-hand"));
  m.join();
  CHECK(m.busy() && !m.canJoin() && client->joins == 0 && m.statusText().contains(QStringLiteral("入場券")));
  m.join();  // a second press while waiting
  rig.gate->flush();
  CHECK(client->joins == 1 && client->last.sdkToken == "sample.sdk.token" && client->last.zak == "sample-zak");
  CHECK(client->last.clientId.empty() && client->last.clientSecret.empty() && client->last.meetingNumber == 12345678901ULL);
  CHECK(m.joinNotice().isEmpty());
  m.leave();

  // No ZAK from the sign-in this time: the one typed by hand is used, and the operator is told.
  rig.gate->answer(QStringLiteral("/v1/zoom/zak"), 502, {{QStringLiteral("error"), QStringLiteral("zoom_failed")}, {QStringLiteral("detail"), QStringLiteral("down")}});
  m.join();
  rig.gate->flush();
  CHECK(client->joins == 2 && client->last.zak == "typed-by-hand" && !m.joinNotice().isEmpty());
  m.leave();

  // The licence does not work: no join, and the reason is on the screen.
  rig.now += 2 * 86400;
  rig.gate->answer(QStringLiteral("/v1/sdk-token"), 403, {{QStringLiteral("error"), QStringLiteral("revoked")}});
  m.join();
  rig.gate->flush();
  CHECK(client->joins == 2 && m.failed() && !m.busy() && m.detail().contains(QStringLiteral("止められています")) && m.canJoin());

  // Zoom keys typed into this PC come first; the server is then not asked at all.
  const int asked = static_cast<int>(rig.gate->asked.size());
  CHECK(m.saveCredentials(QStringLiteral("SampleClientId_01"), QStringLiteral("sample-secret")));
  m.join();
  CHECK(client->joins == 3 && client->last.sdkToken.empty() && client->last.clientId == "SampleClientId_01");
  CHECK(static_cast<int>(rig.gate->asked.size()) == asked);
}

// The real HTTP part, against a tiny server on this machine.
void testHttp() {
  QTcpServer server;
  CHECK(server.listen(QHostAddress::LocalHost));
  QByteArray seen;
  QObject::connect(&server, &QTcpServer::newConnection, [&] {
    QTcpSocket* socket = server.nextPendingConnection();
    QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
      seen += socket->readAll();
      const int headerEnd = static_cast<int>(seen.indexOf("\r\n\r\n"));
      if (headerEnd < 0 || !seen.mid(headerEnd + 4).endsWith('}')) return;
      const QByteArray body = seen.contains("/v1/odd") ? QByteArray("not json") : QByteArray("{\"error\":\"seats_full\",\"seats\":2}");
      socket->write("HTTP/1.1 409 Conflict\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: " +
                    QByteArray::number(body.size()) + "\r\n\r\n" + body);
      socket->disconnectFromHost();
    });
  });
  const std::unique_ptr<GateClient> gate = makeGateClient(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()));
  auto ask = [&](const QString& path, const QString& bearer) {
    GateReply got;
    bool done = false;
    seen.clear();
    gate->post(path, {{QStringLiteral("key"), QStringLiteral("サンプル")}}, bearer, [&](const GateReply& r) {
      got = r;
      done = true;
    });
    CHECK(!done);  // never from inside post()
    QTimer limit;
    limit.setSingleShot(true);
    limit.start(20000);
    while (!done && limit.isActive()) QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    CHECK(done);
    return got;
  };
  const GateReply refused = ask(QStringLiteral("/v1/activate"), QStringLiteral("sample-device-token"));
  CHECK(refused.status == 409 && refused.transport.isEmpty());
  CHECK(refused.body.value(QStringLiteral("error")).toString() == QStringLiteral("seats_full") && refused.body.value(QStringLiteral("seats")).toInt() == 2);
  CHECK(seen.startsWith("POST /v1/activate HTTP/1.1\r\n"));
  CHECK(seen.toLower().contains("authorization: bearer sample-device-token\r\n") && seen.toLower().contains("content-type: application/json\r\n"));
  CHECK(seen.endsWith(QStringLiteral("{\"key\":\"サンプル\"}").toUtf8()));
  const GateReply odd = ask(QStringLiteral("/v1/odd"), QString());
  CHECK(odd.status == 409 && odd.body.isEmpty());
  CHECK(!seen.toLower().contains("authorization:"));
  server.close();
  const GateReply nobody = ask(QStringLiteral("/v1/activate"), QString());
  CHECK(nobody.status == 0 && !nobody.transport.isEmpty() && nobody.body.isEmpty());
}

}  // namespace

int main(int argc, char* argv[]) {
  qputenv("QT_QPA_PLATFORM", "offscreen");
  QGuiApplication qt(argc, argv);
  testLicence();
  testJoinTokens();
  testZoomSignIn();
  testJoining();
  testHttp();
  if (testing::failures() == 0) {
    std::printf("all tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
