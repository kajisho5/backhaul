#include "licence_controller.h"
#include "dictionary_translator.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QStringList>
#include <QTimeZone>
#include <QUuid>

namespace {

constexpr int kSignInSeconds = 600;        // how long the browser part may take
constexpr qint64 kTokenReserveSeconds = 3600;  // ask for a new token when less than this is left

QString randomVerifier() {
  static const char kLetters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  QString text;
  for (int i = 0; i < 64; ++i) {
    text.append(QLatin1Char(kLetters[QRandomGenerator::system()->bounded(static_cast<quint32>(sizeof(kLetters) - 1))]));
  }
  return text;
}

QString challengeOf(const QString& verifier) {
  return QString::fromLatin1(QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256)
                                 .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString dayOf(qint64 seconds) {
  return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::systemTimeZone()).toString(QStringLiteral("yyyy-MM-dd"));
}

}  // namespace

LicenceController::LicenceController(std::unique_ptr<GateClient> gate, std::unique_ptr<SecretStore> licenceStore,
                                     QString deviceName, Opener openUrl, Clock clock, QObject* parent)
    : QObject(parent),
      gate_(std::move(gate)),
      licenceStore_(std::move(licenceStore)),
      machineName_(deviceName.trimmed().left(60)),
      openUrl_(std::move(openUrl)),
      clock_(std::move(clock)) {
  if (machineName_.isEmpty()) machineName_ = QStringLiteral("PC");
  loginTimer_.setInterval(2000);
  connect(&loginTimer_, &QTimer::timeout, this, [this] { pollSignIn(); });
  if (!available()) return;
  // What was saved last time is shown right away; refresh() then asks the server.
  const QJsonObject saved = QJsonDocument::fromJson(licenceStore_->load()).object();
  deviceId_ = saved.value(QStringLiteral("deviceId")).toString();
  deviceToken_ = saved.value(QStringLiteral("token")).toString();
  if (deviceId_.isEmpty() || deviceToken_.isEmpty()) {
    deviceId_.clear();
    deviceToken_.clear();
  } else {
    keyTail_ = saved.value(QStringLiteral("keyTail")).toString();
    shownName_ = saved.value(QStringLiteral("name")).toString();
    seats_ = saved.value(QStringLiteral("seats")).toInt();
    used_ = saved.value(QStringLiteral("used")).toInt();
    expiresAt_ = static_cast<qint64>(saved.value(QStringLiteral("expiresAt")).toDouble());
    zoomSignedIn_ = saved.value(QStringLiteral("zoomSignedIn")).toBool();  // as last seen; refresh() asks again
  }
}

qint64 LicenceController::now() const { return clock_ ? clock_() : QDateTime::currentSecsSinceEpoch(); }

QString LicenceController::expiresText() const {
  if (!active()) return QString();
  return expiresAt_ > 0 ? dayOf(expiresAt_) + T(" まで") : T("期限なし");
}

void LicenceController::say(const QString& message, bool bad) {
  message_ = message;
  messageBad_ = bad;
  emit changed();
}

void LicenceController::sayZoom(const QString& message, bool bad) {
  zoomMessage_ = message;
  zoomMessageBad_ = bad;
  emit changed();
}

void LicenceController::post(const QString& path, const QJsonObject& body, GateClient::Done done) {
  ++pending_;
  emit changed();
  gate_->post(path, body, deviceToken_, [this, done = std::move(done)](const GateReply& reply) {
    --pending_;
    done(reply);
    emit changed();
  });
}

// What a refusal means, in the operator's words. Also acts on the one refusal that changes
// this PC's state: the server no longer knows this PC.
QString LicenceController::explain(const GateReply& reply) {
  if (!reply.transport.isEmpty()) {
    return T("ライセンスのサーバーに届きません。ネットを確かめてください（%1）").arg(reply.transport);
  }
  const QString error = reply.body.value(QStringLiteral("error")).toString();
  if (error == QStringLiteral("unknown_key")) return T("このライセンスキーは見つかりません");
  if (error == QStringLiteral("seats_full")) {
    QStringList names;
    for (const QJsonValue& v : reply.body.value(QStringLiteral("devices")).toArray()) names.push_back(v.toString());
    return T("このキーは%1台すべて使用中です（%2）。ほかのPCで「このPCを外す」を押すと空きます")
        .arg(reply.body.value(QStringLiteral("seats")).toInt())
        .arg(names.join(QStringLiteral("、")));
  }
  if (error == QStringLiteral("revoked")) return T("このキーは止められています");
  if (error == QStringLiteral("expired")) {
    const qint64 end = static_cast<qint64>(reply.body.value(QStringLiteral("expiresAt")).toDouble());
    return end > 0 ? T("期限が切れています（%1 まで）").arg(dayOf(end)) : T("期限が切れています");
  }
  if (error == QStringLiteral("slow_down")) return T("試した回数が多すぎます。10分ほど待ってからもう一度どうぞ");
  if (error == QStringLiteral("not_configured")) return T("サーバー側の準備がまだ終わっていません（Zoomの鍵が入っていません）");
  if (error == QStringLiteral("unknown_device")) {
    forgetLicence();
    return T("このPCの登録が外れています。ライセンスキーを入れ直してください");
  }
  return T("サーバーが思わぬ返事をしました（%1 %2）").arg(reply.status).arg(error);
}

void LicenceController::takeLicence(const QJsonObject& licence) {
  keyTail_ = licence.value(QStringLiteral("keyTail")).toString().left(8);
  seats_ = licence.value(QStringLiteral("seats")).toInt();
  used_ = licence.value(QStringLiteral("used")).toInt();
  expiresAt_ = static_cast<qint64>(licence.value(QStringLiteral("expiresAt")).toDouble());
  shownName_ = licence.value(QStringLiteral("deviceName")).toString().left(60);
  zoomSignedIn_ = licence.value(QStringLiteral("zoomSignedIn")).toBool();
  saveLicence();
}

void LicenceController::saveLicence() {
  QJsonObject o;
  o[QStringLiteral("deviceId")] = deviceId_;
  o[QStringLiteral("token")] = deviceToken_;
  o[QStringLiteral("keyTail")] = keyTail_;
  o[QStringLiteral("name")] = shownName_;
  o[QStringLiteral("seats")] = seats_;
  o[QStringLiteral("used")] = used_;
  o[QStringLiteral("expiresAt")] = static_cast<double>(expiresAt_);
  o[QStringLiteral("zoomSignedIn")] = zoomSignedIn_;
  if (!licenceStore_->save(QJsonDocument(o).toJson(QJsonDocument::Compact))) {
    say(T("ライセンスをこのPCに保存できませんでした。次に起動したとき、もう一度キーが必要です"), true);
  }
}

void LicenceController::forgetLicence() {
  deviceId_.clear();
  deviceToken_.clear();
  keyTail_.clear();
  shownName_.clear();
  seats_ = used_ = 0;
  expiresAt_ = 0;
  sdkToken_.clear();
  sdkTokenExpiresAt_ = 0;
  zoomSignedIn_ = false;
  licenceStore_->clear();
}

void LicenceController::activate(const QString& key) {
  if (!available() || busy() || key.trimmed().isEmpty()) return;
  // The same PC keeps its number, so that entering the key again does not take a second place.
  const QString deviceId = deviceId_.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : deviceId_;
  QJsonObject body;
  body[QStringLiteral("key")] = key.trimmed().left(80);
  body[QStringLiteral("deviceId")] = deviceId;
  body[QStringLiteral("deviceName")] = machineName_;
  post(QStringLiteral("/v1/activate"), body, [this, deviceId](const GateReply& reply) {
    const QString token = reply.body.value(QStringLiteral("deviceToken")).toString();
    if (reply.status != 200 || token.isEmpty()) {
      say(explain(reply), true);
      return;
    }
    deviceId_ = deviceId;
    deviceToken_ = token;
    sdkToken_.clear();
    sdkTokenExpiresAt_ = 0;
    message_.clear();
    messageBad_ = false;
    takeLicence(reply.body.value(QStringLiteral("licence")).toObject());
  });
}

void LicenceController::deactivate() {
  if (!available() || busy() || !active()) return;
  post(QStringLiteral("/v1/deactivate"), {}, [this](const GateReply& reply) {
    // 401: the server has already forgotten this PC, which is what was asked for.
    if (reply.status != 200 && reply.status != 401) {
      say(explain(reply) + T("　このPCはまだ登録されたままです"), true);
      return;
    }
    forgetLicence();
    say(T("このPCを外しました"), false);
  });
}

void LicenceController::refresh() {
  if (!available() || busy() || !active()) return;
  post(QStringLiteral("/v1/status"), {}, [this](const GateReply& reply) {
    if (reply.status != 200) {
      say(explain(reply), true);
      return;
    }
    takeLicence(reply.body.value(QStringLiteral("licence")).toObject());
    const QString problem = reply.body.value(QStringLiteral("problem")).toString();
    if (problem == QStringLiteral("revoked")) {
      say(T("このキーは止められています"), true);
    } else if (problem == QStringLiteral("expired")) {
      say(T("期限が切れています（%1 まで）").arg(dayOf(expiresAt_)), true);
    } else {
      say(QString(), false);
    }
  });
}

// ---- Zoom sign-in ----

void LicenceController::signIn() {
  if (!available() || !active() || zoomWaiting()) {
    if (available() && !active()) sayZoom(T("先にライセンスキーを入れてください"), true);
    return;
  }
  const QString verifier = randomVerifier();
  QJsonObject body;
  body[QStringLiteral("challenge")] = challengeOf(verifier);
  post(QStringLiteral("/v1/zoom/begin"), body, [this, verifier](const GateReply& reply) {
    const QString state = reply.body.value(QStringLiteral("state")).toString();
    const QUrl url(reply.body.value(QStringLiteral("url")).toString());
    if (reply.status != 200 || state.isEmpty()) {
      sayZoom(explain(reply), true);
      return;
    }
    // Only Zoom's own sign-in page is ever opened, whatever the answer says.
    if (url.scheme() != QStringLiteral("https") || url.host() != QStringLiteral("zoom.us")) {
      sayZoom(T("サーバーが思わぬ行き先を返したため、開きませんでした"), true);
      return;
    }
    if (!openUrl_ || !openUrl_(url)) {
      sayZoom(T("ブラウザを開けませんでした"), true);
      return;
    }
    loginState_ = state;
    loginVerifier_ = verifier;
    loginDeadline_ = now() + kSignInSeconds;
    loginTimer_.start();
    sayZoom(T("ブラウザでサインインして「許可」を押してください。終わるとここが変わります"), false);
  });
}

void LicenceController::cancelSignIn() {
  if (!zoomWaiting()) return;
  loginTimer_.stop();
  loginState_.clear();
  loginVerifier_.clear();
  sayZoom(QString(), false);
}

void LicenceController::pollSignIn() {
  if (!zoomWaiting() || busy()) return;
  if (now() > loginDeadline_) {
    cancelSignIn();
    sayZoom(T("時間内にサインインが終わりませんでした。もう一度どうぞ"), true);
    return;
  }
  QJsonObject body;
  body[QStringLiteral("state")] = loginState_;
  body[QStringLiteral("verifier")] = loginVerifier_;
  const QString asked = loginState_;
  post(QStringLiteral("/v1/zoom/collect"), body, [this, asked](const GateReply& reply) {
    if (loginState_ != asked) return;  // cancelled in the meantime
    if (reply.status == 202 || !reply.transport.isEmpty()) return;  // not yet; a network hiccup just waits for the next round
    loginTimer_.stop();
    loginState_.clear();
    loginVerifier_.clear();
    if (reply.status == 200 && reply.body.value(QStringLiteral("signedIn")).toBool()) {
      zoomSignedIn_ = true;
      saveLicence();
      sayZoom(QString(), false);
    } else if (reply.status == 410) {
      sayZoom(T("許可されなかったため、サインインしていません"), true);
    } else if (reply.status == 404) {
      sayZoom(T("時間内にサインインが終わりませんでした。もう一度どうぞ"), true);
    } else if (reply.body.value(QStringLiteral("error")).toString() == QStringLiteral("zoom_failed")) {
      sayZoom(T("Zoomがサインインを受け付けませんでした（%1）").arg(reply.body.value(QStringLiteral("detail")).toString()), true);
    } else {
      sayZoom(explain(reply), true);
    }
  });
}

void LicenceController::signOut() {
  if (!available() || !active() || !zoomSignedIn() || busy()) return;
  post(QStringLiteral("/v1/zoom/signout"), {}, [this](const GateReply& reply) {
    if (reply.status != 200) {
      sayZoom(explain(reply) + T("　サインインはそのままです"), true);
      return;
    }
    zoomSignedIn_ = false;
    saveLicence();
    // Signed out for every PC of the licence either way; say so when Zoom could not be told.
    sayZoom(reply.body.value(QStringLiteral("revoked")).toBool()
                ? T("サインアウトしました（このライセンスのPCすべて）")
                : T("サインアウトしました（Zoom側の取り消しは確認できませんでした）"),
            false);
  });
}

// ---- joining ----

void LicenceController::prepareJoin(std::function<void(const Entry&)> done) {
  Entry entry;
  if (!available() || !active()) {
    entry.error = T("ライセンスキーが入っていません");
    done(entry);
    return;
  }
  if (!sdkToken_.isEmpty() && sdkTokenExpiresAt_ - now() > kTokenReserveSeconds) {
    entry.ok = true;
    entry.sdkToken = sdkToken_;
    addZak(entry, std::move(done));
    return;
  }
  post(QStringLiteral("/v1/sdk-token"), {}, [this, done = std::move(done)](const GateReply& reply) {
    Entry entry;
    const QString token = reply.body.value(QStringLiteral("token")).toString();
    if (reply.status == 200 && !token.isEmpty()) {
      sdkToken_ = token;
      sdkTokenExpiresAt_ = static_cast<qint64>(reply.body.value(QStringLiteral("expiresAt")).toDouble());
    } else if (!reply.transport.isEmpty() && !sdkToken_.isEmpty() && sdkTokenExpiresAt_ > now()) {
      // The server is out of reach, but the token from earlier today is still good.
    } else {
      entry.error = explain(reply);
      say(entry.error, true);
      done(entry);
      return;
    }
    entry.ok = true;
    entry.sdkToken = sdkToken_;
    addZak(entry, done);
  });
}

// The sign-in belongs to the licence, not to this PC: another PC may have signed in or out
// since this one last looked. So the server is always asked, and what this PC believed
// only decides whether a missing ZAK is worth a notice.
void LicenceController::addZak(Entry entry, std::function<void(const Entry&)> done) {
  const bool believed = zoomSignedIn();
  post(QStringLiteral("/v1/zoom/zak"), {}, [this, entry, believed, done = std::move(done)](const GateReply& reply) mutable {
    const QString zak = reply.body.value(QStringLiteral("zak")).toString();
    const QString error = reply.body.value(QStringLiteral("error")).toString();
    if (reply.status == 200 && !zak.isEmpty()) {
      entry.zak = zak;
      if (!zoomSignedIn_) {  // signed in on another PC in the meantime
        zoomSignedIn_ = true;
        saveLicence();
        sayZoom(QString(), false);
      }
    } else if (error == QStringLiteral("signed_out") || error == QStringLiteral("not_signed_in")) {
      if (believed || error == QStringLiteral("signed_out")) {
        zoomSignedIn_ = false;
        saveLicence();
        entry.notice = T("Zoomのサインインが切れました。ほかのアカウントの会議に入るには、もう一度サインインしてください");
        sayZoom(entry.notice, true);
      }
    } else if (!believed && error != QStringLiteral("zoom_failed")) {
      // Nobody is known to have signed in, and the server did not say otherwise: nothing to tell.
    } else {
      entry.notice = T("Zoomの許可（ZAK）を取れませんでした。ほかのアカウントの会議には入れないことがあります（%1）")
                         .arg(reply.transport.isEmpty() ? reply.body.value(QStringLiteral("detail")).toString() : reply.transport);
      sayZoom(entry.notice, true);
    }
    done(entry);
  });
}
