// Updates as the screens see them, with a made-up web and a made-up updater.
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonObject>
#include <vector>

#include "app_state.h"
#include "check.h"
#include "update_controller.h"

namespace {

struct FakeUpdater final : backhaul::Updater {
  struct Seen {
    std::string version;
    Hooks hooks;
    int installs = 0;
  };
  explicit FakeUpdater(Seen& seen) : seen_(seen) {}
  void start(const std::string& version, Hooks hooks) override {
    seen_.version = version;
    seen_.hooks = std::move(hooks);
  }
  void install() override { ++seen_.installs; }
  Seen& seen_;
};

// Answers later, like a network would: the test decides when and what.
struct FakeWeb {
  std::vector<QUrl> asked;
  std::vector<std::function<void(bool, const QByteArray&)>> waiting;
  UpdateController::Fetch fetch() {
    return [this](const QUrl& url, std::function<void(bool, const QByteArray&)> done) {
      asked.push_back(url);
      waiting.push_back(std::move(done));
    };
  }
  void answer(bool ok, const QByteArray& body) {
    auto done = std::move(waiting.front());
    waiting.erase(waiting.begin());
    done(ok, body);
  }
};

const QUrl kLatest(QStringLiteral("https://updates.example.invalid/latest.json"));
const QByteArray kNewer = R"({"version":"0.2.0","notes":{"en":"Sample change","ja":"見本の変更"}})";

}  // namespace

int main(int argc, char* argv[]) {
  QGuiApplication qtApp(argc, argv);
  const auto clock = [] { return QStringLiteral("14:02"); };

  {  // a newer version: found at the start, offered, installed only when asked
    AppState app;
    FakeUpdater::Seen seen;
    FakeWeb web;
    UpdateController update(app, QStringLiteral("0.1.0"), kLatest, std::make_unique<FakeUpdater>(seen), web.fetch(), clock);
    CHECK(seen.version == "0.1.0");  // the updater knows which version this is
    CHECK(update.status() == QStringLiteral("idle") && !update.noticeVisible() && update.selfUpdating());
    update.start();
    CHECK(web.asked.size() == 1 && web.asked[0] == kLatest);
    CHECK(update.status() == QStringLiteral("checking") && !update.canCheck());
    web.answer(true, kNewer);
    CHECK(update.status() == QStringLiteral("available"));
    CHECK(update.latestVersion() == QStringLiteral("0.2.0") && update.checkedAt() == QStringLiteral("14:02"));
    CHECK(update.notes() == QStringLiteral("Sample change"));
    CHECK(update.noticeVisible() && update.canInstall());
    CHECK(seen.installs == 0);  // finding is not installing
    update.install();
    CHECK(seen.installs == 1);

    // The notes follow the language shown.
    app.setLanguage(QStringLiteral("ja"));
    CHECK(update.notes() == QString::fromUtf8("見本の変更"));
  }

  {  // in a meeting: no notice, no look, no install, and the program may not be closed
    AppState app;
    FakeUpdater::Seen seen;
    FakeWeb web;
    UpdateController update(app, QStringLiteral("0.1.0"), kLatest, std::make_unique<FakeUpdater>(seen), web.fetch(), clock);
    CHECK(seen.hooks.mayClose());
    update.checkNow();
    web.answer(true, kNewer);
    update.setInMeeting(true);
    CHECK(update.status() == QStringLiteral("available"));
    CHECK(!update.noticeVisible() && !update.canInstall() && !update.canCheck());
    CHECK(!seen.hooks.mayClose());
    update.install();
    update.checkNow();
    CHECK(seen.installs == 0 && web.asked.size() == 1);
    // The meeting is over: everything is offered again.
    update.setInMeeting(false);
    CHECK(update.noticeVisible() && update.canInstall() && seen.hooks.mayClose());

    // A meeting that begins while the answer is on its way: the answer is kept, not shown.
    update.checkNow();
    update.setInMeeting(true);
    web.answer(true, kNewer);
    CHECK(update.status() == QStringLiteral("available") && !update.noticeVisible());
  }

  {  // "later" hides the strip for this run; the settings page still offers the version
    AppState app;
    FakeUpdater::Seen seen;
    FakeWeb web;
    UpdateController update(app, QStringLiteral("0.1.0"), kLatest, std::make_unique<FakeUpdater>(seen), web.fetch(), clock);
    update.checkNow();
    web.answer(true, kNewer);
    int changes = 0;
    QObject::connect(&update, &UpdateController::changed, &qtApp, [&changes] { ++changes; });
    update.later();
    update.later();
    CHECK(changes == 1);
    CHECK(!update.noticeVisible() && update.canInstall() && update.status() == QStringLiteral("available"));
  }

  {  // nothing newer, and answers that cannot be trusted
    AppState app;
    FakeWeb web;
    UpdateController update(app, QStringLiteral("0.2.0"), kLatest, nullptr, web.fetch(), clock);
    CHECK(!update.selfUpdating());
    update.checkNow();
    web.answer(true, kNewer);  // the same version
    CHECK(update.status() == QStringLiteral("current") && !update.noticeVisible() && update.latestVersion().isEmpty());
    update.checkNow();
    web.answer(true, R"({"version":"0.1.9"})");  // an older one is not offered
    CHECK(update.status() == QStringLiteral("current"));
    for (const QByteArray& bad : {QByteArray("not json"), QByteArray("{}"), QByteArray(R"({"version":"latest"})"), QByteArray(R"({"version":7})"), QByteArray("[]")}) {
      update.checkNow();
      web.answer(true, bad);
      CHECK(update.status() == QStringLiteral("failed") && !update.noticeVisible());
    }
    update.checkNow();
    web.answer(false, kNewer);  // the web could not be reached: what came with it is not used
    CHECK(update.status() == QStringLiteral("failed") && update.checkedAt() == QStringLiteral("14:02"));
    // A newer version, in a build without an updater: shown, but nothing to install.
    update.checkNow();
    web.answer(true, R"({"version":"v0.3.0","notes":{"en":"Only English"}})");
    CHECK(update.status() == QStringLiteral("available") && update.latestVersion() == QStringLiteral("0.3.0"));
    CHECK(update.noticeVisible() && !update.canInstall());
    app.setLanguage(QStringLiteral("ja"));
    CHECK(update.notes() == QStringLiteral("Only English"));  // no Japanese notes: the English ones
    update.install();                                          // must not crash
  }

  {  // the setting "look when the program starts", and a build that cannot look
    AppState app;
    FakeWeb web;
    UpdateController update(app, QStringLiteral("0.1.0"), kLatest, nullptr, web.fetch(), clock);
    CHECK(app.checksUpdates());
    app.setChecksUpdates(false);
    update.start();
    CHECK(web.asked.empty() && update.status() == QStringLiteral("idle"));
    update.checkNow();  // by hand it still works
    CHECK(web.asked.size() == 1);

    UpdateController none(app, QStringLiteral("0.1.0"), kLatest, nullptr, {}, clock);
    CHECK(!none.canCheck());
    none.start();
    none.checkNow();
    CHECK(none.status() == QStringLiteral("idle"));
  }

  {  // the updater asks to close the program: passed on, on this thread
    AppState app;
    FakeUpdater::Seen seen;
    FakeWeb web;
    UpdateController update(app, QStringLiteral("0.1.0"), kLatest, std::make_unique<FakeUpdater>(seen), web.fetch(), clock);
    int asked = 0;
    QObject::connect(&update, &UpdateController::closeRequested, &qtApp, [&asked] { ++asked; });
    seen.hooks.close();
    CHECK(asked == 0);  // not from inside the updater's call
    QCoreApplication::processEvents();
    CHECK(asked == 1);
  }

  {  // an answer that arrives after the controller is gone is dropped
    AppState app;
    FakeWeb web;
    {
      UpdateController update(app, QStringLiteral("0.1.0"), kLatest, nullptr, web.fetch(), clock);
      update.checkNow();
    }
    web.answer(true, kNewer);
  }

  {  // the setting is saved, and files from before the updater keep it switched on
    AppState app;
    app.loadDefaults();
    app.setChecksUpdates(false);
    AppState other;
    CHECK(other.applySettings(app.settingsJson()) && !other.checksUpdates());
    QJsonObject old = app.settingsJson();
    old.remove(QStringLiteral("checksUpdates"));
    CHECK(other.applySettings(old) && other.checksUpdates());
    old[QStringLiteral("checksUpdates")] = QStringLiteral("yes");
    CHECK(!other.applySettings(old));
  }

  if (testing::failures() == 0) {
    std::printf("all update controller tests passed\n");
    return 0;
  }
  std::printf("%d check(s) failed\n", testing::failures());
  return 1;
}
