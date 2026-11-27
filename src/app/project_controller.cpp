#include "project_controller.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "app_state.h"
#include "file_pick.h"
#include "plugin_controller.h"

namespace {
constexpr qint64 kMaxProjectBytes = 256 * 1024 * 1024;  // plug-in settings can be large, but not this large
}

ProjectController::ProjectController(AppState& app, PluginController& plugins, std::function<void()> afterOpen,
                                     QObject* parent)
    : QObject(parent), app_(app), plugins_(plugins), afterOpen_(std::move(afterOpen)) {}

bool ProjectController::canPick() const { return canPickFiles(); }

void ProjectController::say(const QString& message, bool failed) {
  message_ = message;
  failed_ = failed;
  emit changed();
}

void ProjectController::save() {
  const QString suggested = fileName_.isEmpty() ? QStringLiteral("project.backhaul") : fileName_;
  const QString path = pickSaveFile(QStringLiteral("プロジェクトを保存"), suggested);
  if (!path.isEmpty()) saveTo(path);
}

void ProjectController::open() {
  const QString path = pickOpenFile(QStringLiteral("プロジェクトを開く"));
  if (!path.isEmpty()) openFrom(path);
}

bool ProjectController::saveTo(const QString& path) {
  plugins_.capture();  // the plug-ins' settings as they are right now
  QSaveFile file(path);  // the old file stays whole unless the new one is written completely
  const QByteArray text = QJsonDocument(app_.settingsJson()).toJson(QJsonDocument::Indented);
  if (!file.open(QIODevice::WriteOnly) || file.write(text) != text.size() || !file.commit()) {
    say(QStringLiteral("保存できませんでした：%1").arg(file.errorString()), true);
    return false;
  }
  fileName_ = QFileInfo(path).fileName();
  say(QStringLiteral("「%1」に保存しました").arg(fileName_), false);
  return true;
}

bool ProjectController::openFrom(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    say(QStringLiteral("開けませんでした：%1").arg(file.errorString()), true);
    return false;
  }
  if (file.size() > kMaxProjectBytes) {
    say(QStringLiteral("開けませんでした：ファイルが大きすぎます"), true);
    return false;
  }
  QJsonParseError parse{};
  const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parse);
  if (parse.error != QJsonParseError::NoError || !document.isObject()) {
    say(QStringLiteral("開けませんでした：backhaulのプロジェクトではないか、壊れています"), true);
    return false;
  }
  // applySettings checks everything first and changes nothing when the file is not acceptable.
  if (!app_.applySettings(document.object())) {
    say(QStringLiteral("開けませんでした：このバージョンのbackhaulで読める内容ではありません"), true);
    return false;
  }
  plugins_.restore();
  if (afterOpen_) afterOpen_();
  emit app_.settingsEdited();  // from now on this is the set-up that is kept between runs
  app_.notifyChanged();
  fileName_ = QFileInfo(path).fileName();
  say(QStringLiteral("「%1」を開きました").arg(fileName_), false);
  return true;
}
