#include "settings_saver.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include "app_state.h"

SettingsSaver::SettingsSaver(AppState& app, QString path, QObject* parent)
    : QObject(parent), app_(app), path_(std::move(path)) {
  timer_.setSingleShot(true);
  timer_.setInterval(400);
  connect(&timer_, &QTimer::timeout, this, [this] { flush(); });
  connect(&app_, &AppState::settingsEdited, this, [this] {
    pending_ = true;
    timer_.start();
  });
}

QString SettingsSaver::defaultPath() {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
      .filePath(QStringLiteral("settings.json"));
}

SettingsSaver::LoadResult SettingsSaver::load() {
  QFile file(path_);
  if (!file.exists()) {
    app_.loadDefaults();
    return LoadResult::NoFile;
  }
  bool ok = false;
  if (file.open(QIODevice::ReadOnly)) {
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    ok = error.error == QJsonParseError::NoError && doc.isObject() && app_.applySettings(doc.object());
  }
  if (ok) return LoadResult::Loaded;

  // Keep the damaged file so nothing the user typed is lost for good.
  const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
  const QFileInfo info(path_);
  const QString aside = info.dir().filePath(QStringLiteral("settings.unreadable-%1.json").arg(stamp));
  asidePath_ = QFile::rename(path_, aside) ? aside : QString();
  app_.loadDefaults();
  return LoadResult::Unreadable;
}

bool SettingsSaver::flush() {
  timer_.stop();
  if (!pending_) return true;
  if (!write()) {
    app_.notice_ = QStringLiteral("設定を保存できませんでした");
    app_.notifyChanged();
    return false;
  }
  pending_ = false;
  if (app_.notice_ == QStringLiteral("設定を保存できませんでした")) {
    app_.notice_.clear();
    app_.notifyChanged();
  }
  return true;
}

bool SettingsSaver::write() {
  // A damaged file that could not be set aside must not be replaced.
  if (asidePath_.isEmpty() && QFile::exists(path_)) {
    QFile existing(path_);
    if (!existing.open(QIODevice::ReadOnly)) return false;
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(existing.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject() ||
        doc.object().value(QStringLiteral("format")).toInt() != 1) {
      return false;
    }
  }
  if (!QDir().mkpath(QFileInfo(path_).absolutePath())) return false;
  QSaveFile out(path_);
  if (!out.open(QIODevice::WriteOnly)) return false;
  const QByteArray bytes = QJsonDocument(app_.settingsJson()).toJson(QJsonDocument::Indented);
  if (out.write(bytes) != bytes.size()) {
    out.cancelWriting();
    return false;
  }
  return out.commit();
}
