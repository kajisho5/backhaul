#include "update_controller.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTime>

#include "app_state.h"
#include "core/update_policy.h"

namespace {
constexpr qint64 kMaxAnswerBytes = 64 * 1024;
constexpr int kTimeoutMs = 15000;
}  // namespace

UpdateController::UpdateController(AppState& app, QString currentVersion, QUrl latestUrl,
                                   std::unique_ptr<backhaul::Updater> updater, Fetch fetch, Clock clock, QObject* parent)
    : QObject(parent),
      app_(app),
      currentVersion_(std::move(currentVersion)),
      latestUrl_(std::move(latestUrl)),
      updater_(std::move(updater)),
      fetch_(std::move(fetch)),
      clock_(clock ? std::move(clock) : [] { return QTime::currentTime().toString(QStringLiteral("HH:mm")); }) {
  connect(&app_, &AppState::languageChanged, this, &UpdateController::changed);  // the notes follow the language
  if (updater_ != nullptr) {
    // Both are called from the updater's thread: the flag is atomic, and closing is
    // handed over to this object's thread. `self` covers the updater outliving nothing:
    // it is destroyed with this object, before the flag and the pointer go.
    const QPointer<UpdateController> self(this);
    const auto mayClose = mayClose_;
    updater_->start(currentVersion_.toStdString(),
                    {[mayClose] { return mayClose->load(); },
                     [self] {
                       if (self) QMetaObject::invokeMethod(self.data(), &UpdateController::closeRequested, Qt::QueuedConnection);
                     }});
  }
}

QString UpdateController::notes() const {
  const QString& own = app_.language() == QStringLiteral("ja") ? notesJa_ : notesEn_;
  return own.isEmpty() ? notesEn_ : own;
}

bool UpdateController::noticeVisible() const {
  return status_ == QStringLiteral("available") &&
         backhaul::showsUpdateNotice(currentVersion_.toStdString(), latestVersion_.toStdString(), inMeeting_, saidLater_);
}

void UpdateController::start() {
  if (app_.checksUpdates()) checkNow();
}

void UpdateController::setInMeeting(bool inMeeting) {
  if (inMeeting == inMeeting_) return;
  inMeeting_ = inMeeting;
  mayClose_->store(!inMeeting);
  emit changed();
}

void UpdateController::checkNow() {
  if (!canCheck()) return;
  status_ = QStringLiteral("checking");
  emit changed();
  const QPointer<UpdateController> self(this);
  fetch_(latestUrl_, [self](bool ok, const QByteArray& body) {
    if (self) self->received(ok, body);
  });
}

void UpdateController::received(bool ok, const QByteArray& body) {
  checkedAt_ = clock_();
  const QJsonObject answer = ok ? QJsonDocument::fromJson(body).object() : QJsonObject();
  const QString version = answer.value(QStringLiteral("version")).toString();
  const auto offered = backhaul::parseVersion(version.toStdString());
  const auto have = backhaul::parseVersion(currentVersion_.toStdString());
  if (!offered.has_value() || !have.has_value()) {
    status_ = QStringLiteral("failed");
    emit changed();
    return;
  }
  if (backhaul::isNewer(*offered, *have)) {
    const QJsonObject notes = answer.value(QStringLiteral("notes")).toObject();
    latestVersion_ = version.startsWith(QLatin1Char('v')) ? version.mid(1) : version;
    notesEn_ = notes.value(QStringLiteral("en")).toString().left(4000);
    notesJa_ = notes.value(QStringLiteral("ja")).toString().left(4000);
    status_ = QStringLiteral("available");
  } else {
    latestVersion_.clear();
    notesEn_.clear();
    notesJa_.clear();
    status_ = QStringLiteral("current");
  }
  emit changed();
}

void UpdateController::later() {
  if (saidLater_) return;
  saidLater_ = true;
  emit changed();
}

void UpdateController::install() {
  if (canInstall()) updater_->install();
}

void UpdateController::showForPicture(const QString& version) {
  latestVersion_ = version;
  notesEn_ = QStringLiteral("The release notes of the new version appear here.");
  notesJa_ = notesEn_;
  status_ = QStringLiteral("available");
  checkedAt_ = QStringLiteral("14:02");
  emit changed();
}

UpdateController::Fetch makeNetworkFetch(QObject* owner) {
  auto* network = new QNetworkAccessManager(owner);
  return [network](const QUrl& url, std::function<void(bool, const QByteArray&)> done) {
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kTimeoutMs);
    QNetworkReply* reply = network->get(request);
    // Far more than the file can be: stop instead of filling memory.
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
      if (received > kMaxAnswerBytes) reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done = std::move(done)] {
      const bool ok = reply->error() == QNetworkReply::NoError && reply->url().scheme() == QStringLiteral("https");
      done(ok, ok ? reply->readAll() : QByteArray());
      reply->deleteLater();
    });
  };
}
