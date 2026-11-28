#include <QCommandLineParser>
#include "dictionary_translator.h"
#include <QDesktopServices>
#include <QFileInfo>
#include <QSysInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <qqml.h>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <atomic>
#include <cstdio>
#include <memory>

#include "app_state.h"
#include "audio_controller.h"
#include "check_controller.h"
#include "demo_source.h"
#include "frame_item.h"
#include "frame_store.h"
#include "licence_controller.h"
#include "meeting_controller.h"
#include "plugin_controller.h"
#include "plugins/plugin_host.h"
#include "project_controller.h"
#include "return_controller.h"
#include "settings_saver.h"
#include "system_facts.h"
#include "update/updater.h"
#include "update_controller.h"

int main(int argc, char* argv[]) {
  QGuiApplication qtApp(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("backhaul"));
  QGuiApplication::setApplicationVersion(QStringLiteral(BACKHAUL_VERSION));
  QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icon-256.png")));

  QCommandLineParser args;
  args.addHelpOption();
  args.addVersionOption();
  const QCommandLineOption demoOpt(QStringLiteral("demo"), QStringLiteral("Show made-up people instead of a meeting."));
  const QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                                   QStringLiteral("Save a picture of the operator screen to <file> and quit."),
                                   QStringLiteral("file"));
  const QCommandLineOption sizeOpt(QStringLiteral("window-size"),
                                   QStringLiteral("Open the window at <width>x<height>, for example 1280x720."),
                                   QStringLiteral("size"));
  args.addOption(demoOpt);
  args.addOption(shotOpt);
  args.addOption(sizeOpt);
  const QCommandLineOption pageOpt(QStringLiteral("settings"),
                                   QStringLiteral("Start on a settings page: zoom, slots, audio, video, return or project."), QStringLiteral("page"));
  args.addOption(pageOpt);
  const QCommandLineOption languageOpt(QStringLiteral("language"), QStringLiteral("For pictures: en or ja."), QStringLiteral("en|ja"));
  args.addOption(languageOpt);
  const QCommandLineOption returnOpt(QStringLiteral("return"),
                                     QStringLiteral("For pictures of the all-in-one role: select the return tile, sending on or off."),
                                     QStringLiteral("on|off"));
  args.addOption(returnOpt);
  const QCommandLineOption updateOpt(QStringLiteral("update"),
                                     QStringLiteral("For pictures: show <version> as a newer version that was found."),
                                     QStringLiteral("version"));
  args.addOption(updateOpt);
  const QCommandLineOption lockOpt(QStringLiteral("locked"), QStringLiteral("For pictures: start with the show lock on."));
  args.addOption(lockOpt);
  const QCommandLineOption recoverOpt(QStringLiteral("recover"), QStringLiteral("For pictures: show the \"coming back\" strip with sample lines."));
  args.addOption(recoverOpt);
  const QCommandLineOption roleOpt(QStringLiteral("role"),
                                   QStringLiteral("Start in a role: all, camera, twoshot, slide, audio or return; \"ask\" shows the role question."),
                                   QStringLiteral("role"));
  args.addOption(roleOpt);
  args.process(qtApp);
  if (args.isSet(roleOpt)) {
    const QString role = args.value(roleOpt);
    if (role != QStringLiteral("all") && role != QStringLiteral("camera") && role != QStringLiteral("twoshot") &&
        role != QStringLiteral("slide") && role != QStringLiteral("audio") && role != QStringLiteral("return") &&
        role != QStringLiteral("ask")) {
      std::fprintf(stderr, "--role wants all, camera, twoshot, slide, audio, return or ask\n");
      return 5;
    }
  }

  // English unless the settings say Japanese. Installed before anything makes a text, so
  // that names made up for a first start are English too.
  DictionaryTranslator english;
  QCoreApplication::installTranslator(&english);

  AppState state;
  std::unique_ptr<SettingsSaver> saver;
  if (args.isSet(demoOpt)) {
    // Made-up people: nothing is read from or written to the settings file.
    loadDemo(state);
  } else {
    saver = std::make_unique<SettingsSaver>(state, SettingsSaver::defaultPath());
    state.savesSettings_ = true;
    if (saver->load() == SettingsSaver::LoadResult::Unreadable) {
      state.notice_ = saver->asidePath().isEmpty()
                          ? T("設定ファイルを読めませんでした。初期設定で動いています（保存はしません）")
                          : T("設定ファイルを読めなかったため、初期設定に戻しました（元のファイルは残してあります）");
    }
    state.notifyChanged();
    QObject::connect(&qtApp, &QCoreApplication::aboutToQuit, saver.get(), [&saver] { saver->flush(); });
  }

  if (args.isSet(roleOpt)) {
    if (args.value(roleOpt) != QStringLiteral("ask")) state.setRole(args.value(roleOpt));
  } else if (args.isSet(demoOpt)) {
    state.setRole(QStringLiteral("all"));  // the demo shows the full operator screen unless told otherwise
  }

  // Demo mode never joins a meeting and never touches saved credentials.
  const bool demo = args.isSet(demoOpt);
  FrameStore frames;
  FrameItem::setStore(&frames);
  qmlRegisterType<FrameItem>("Backhaul.Video", 1, 0, "FrameItem");
  MeetingController meeting(state, frames, demo ? nullptr : backhaul::makeMeetingClient(),
                            demo ? nullptr : makeCredentialStore());

  // Licence and Zoom sign-in. BACKHAUL_GATE_URL points a test set-up at another server.
  const QString gateUrl = qEnvironmentVariable("BACKHAUL_GATE_URL", QStringLiteral(BACKHAUL_GATE_URL));
  LicenceController licence(demo ? nullptr : makeGateClient(gateUrl),
                            demo ? nullptr : makeSecretStore(QStringLiteral("backhaul/licence")),
                            QSysInfo::machineHostName(), [](const QUrl& url) { return QDesktopServices::openUrl(url); });
  meeting.setLicence(&licence);
  licence.refresh();

  // While the operator has paused sending, the sound stays here too. Declared before
  // `audio` so that it is still there when audio's sending thread makes its last call.
  std::atomic<bool> sendPaused{false};
  AudioController audio(state, demo ? nullptr : backhaul::makeAudioOutput(),
                        demo ? nullptr : backhaul::makeAsioOutput(), demo ? nullptr : backhaul::makeAudioInput());
  // `meeting` is created before `audio` and so outlives it and its sending thread.
  audio.setSender([&meeting, &sendPaused](const std::int16_t* samples, std::size_t frames, int channels) {
    return !sendPaused.load(std::memory_order_relaxed) && meeting.sendAudio(samples, frames, channels);
  });
  // Plug-ins. The rows they sit in belong to `audio`, which is created first and so is
  // still there when this object takes its plug-ins out of them at the end.
  PluginController plugins(state, audio.mixer(), audio.micEffects(), demo ? nullptr : backhaul::makePluginHost(),
                           demo ? QString() : QFileInfo(SettingsSaver::defaultPath()).absolutePath());
  if (!demo) {
    plugins.restore();
    // The plug-ins' own settings are asked for once more right before the last save.
    QObject::connect(&qtApp, &QCoreApplication::aboutToQuit, &plugins, [&plugins, &saver] {
      plugins.capture();
      if (saver != nullptr) saver->flush();
    });
    audio.startSaved();
  }

  ReturnController returning(state, frames, demo ? nullptr : backhaul::makeVideoInput());
  returning.setSender([&meeting](const std::uint8_t* i420, int width, int height) {
    return meeting.sendVideo(i420, width, height);
  });
  if (!demo) returning.startSaved();
  QObject::connect(&returning, &ReturnController::changed, &qtApp,
                   [&returning, &sendPaused] { sendPaused = returning.paused(); });
  meeting.setMixer(audio.mixer());
  ProjectController project(state, plugins, [&audio, &returning] {
    audio.reloadSaved();
    returning.reloadSaved();
  });

  // Updates. The demo neither looks on the web nor installs anything.
  UpdateController update(state, QStringLiteral(BACKHAUL_VERSION), QUrl(QStringLiteral(BACKHAUL_UPDATE_LATEST_URL)),
                          demo ? nullptr : backhaul::makeUpdater(), demo ? UpdateController::Fetch() : makeNetworkFetch(&qtApp));
  // While this PC is in a meeting (or coming back to one) Windows may neither sleep nor
  // switch the screens off; outside meetings it is left to do as it is set up.
  const auto tellUpdate = [&update, &meeting, demo] {
    update.setInMeeting(meeting.busy());
    if (!demo) keepAwake(meeting.busy());
  };
  QObject::connect(&meeting, &MeetingController::changed, &update, tellUpdate);
  tellUpdate();
  // The installer is about to run: leave like on a normal quit, so that everything is saved.
  QObject::connect(&update, &UpdateController::closeRequested, &qtApp, &QCoreApplication::quit);
  // The check before the show. The demo shows a sample result.
  CheckController check(state, meeting, demo ? nullptr : &licence, demo ? nullptr : &audio, demo ? nullptr : &update);
  if (demo) check.showForPicture();
  if (args.isSet(updateOpt)) {
    update.showForPicture(args.value(updateOpt));
  } else if (!args.isSet(shotOpt)) {
    update.start();
  }

  QQmlApplicationEngine engine;
  const auto applyLanguage = [&state, &english, &engine] {
    if (state.language() == QStringLiteral("ja")) {
      QCoreApplication::removeTranslator(&english);
    } else {
      QCoreApplication::installTranslator(&english);
    }
    engine.retranslate();  // texts already on screen follow at once
  };
  QObject::connect(&state, &AppState::languageChanged, &qtApp, applyLanguage);
  if (args.isSet(languageOpt)) state.setLanguage(args.value(languageOpt));
  applyLanguage();
  engine.rootContext()->setContextProperty(QStringLiteral("meeting"), &meeting);
  engine.rootContext()->setContextProperty(QStringLiteral("audio"), &audio);
  engine.rootContext()->setContextProperty(QStringLiteral("returning"), &returning);
  engine.rootContext()->setContextProperty(QStringLiteral("licence"), &licence);
  engine.rootContext()->setContextProperty(QStringLiteral("plugins"), &plugins);
  engine.rootContext()->setContextProperty(QStringLiteral("project"), &project);
  engine.rootContext()->setContextProperty(QStringLiteral("updates"), &update);  // not "update": every QML item has a function of that name
  engine.rootContext()->setContextProperty(QStringLiteral("check"), &check);
  engine.rootContext()->setContextProperty(QStringLiteral("app"), &state);
  QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &qtApp,
                   [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
  engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Backhaul/qml/Main.qml")));
  if (engine.rootObjects().isEmpty()) return 2;

  auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
  if (window == nullptr) {
    std::fprintf(stderr, "the main window was not created\n");
    return 3;
  }
  if (!args.isSet(demoOpt)) {
    // Keep the monitor list current: monitors come and go, and the window can be moved.
    auto publishScreens = [&state, window] {
      QVector<AppState::ScreenInfo> list;
      const auto screens = QGuiApplication::screens();
      for (const QScreen* s : screens) {
        const int number = static_cast<int>(list.size()) + 1;
        // Some systems give no name; the position in the list is then the only handle.
        const QString key = s->name().isEmpty() ? QStringLiteral("monitor-%1").arg(number) : s->name();
        QString title = s->name().isEmpty() ? T("モニター%1").arg(number) : s->name();
        if (title.startsWith(QStringLiteral("\\\\.\\"))) title.remove(0, 4);  // "\\.\DISPLAY2" -> "DISPLAY2"
        if (!s->model().isEmpty() && s->model() != s->name()) title += QStringLiteral("（%1）").arg(s->model());
        const QSize pixels = s->size() * s->devicePixelRatio();
        const QString mode = QStringLiteral("%1×%2・%3Hz")
                                 .arg(pixels.width())
                                 .arg(pixels.height())
                                 .arg(QString::number(s->refreshRate(), 'g', 4));
        list.push_back({key, title, mode});
      }
      const int own = window->screen() != nullptr ? static_cast<int>(screens.indexOf(window->screen())) : -1;
      state.setScreens(list, own >= 0 ? list[own].key : QString());
    };
    publishScreens();
    QObject::connect(&qtApp, &QGuiApplication::screenAdded, window, publishScreens);
    QObject::connect(&qtApp, &QGuiApplication::screenRemoved, window, publishScreens);
    QObject::connect(window, &QWindow::screenChanged, window, publishScreens);
  }

  if (args.isSet(sizeOpt)) {
    const QStringList parts = args.value(sizeOpt).split(QLatin1Char('x'));
    bool okW = false, okH = false;
    const int w = parts.size() == 2 ? parts[0].toInt(&okW) : 0;
    const int h = parts.size() == 2 ? parts[1].toInt(&okH) : 0;
    if (!okW || !okH || w < 320 || h < 240 || w > 16384 || h > 16384) {
      std::fprintf(stderr, "--window-size wants <width>x<height>, for example 1280x720\n");
      return 5;
    }
    window->resize(w, h);
  }

  if (args.isSet(pageOpt)) {
    const QString page = args.value(pageOpt);
    if (page != QStringLiteral("slots") && page != QStringLiteral("audio") && page != QStringLiteral("video") &&
        page != QStringLiteral("zoom") && page != QStringLiteral("project") && page != QStringLiteral("return") &&
        page != QStringLiteral("software") && page != QStringLiteral("check")) {
      std::fprintf(stderr, "--settings wants check, zoom, slots, audio, video, return, project or software\n");
      return 5;
    }
    window->setProperty("settingsTab", page);
    window->setProperty("settingsOpen", true);
  }

  if (args.isSet(recoverOpt)) window->setProperty("recoverSample", true);
  if (args.isSet(lockOpt)) state.setLocked(true);

  if (args.isSet(returnOpt)) {
    const QString send = args.value(returnOpt);
    if (send != QStringLiteral("on") && send != QStringLiteral("off")) {
      std::fprintf(stderr, "--return wants on or off\n");
      return 5;
    }
    state.setReturnInAll(send == QStringLiteral("on"));
    window->setProperty("returnPicked", true);
  }

  if (args.isSet(shotOpt)) {
    const QString path = args.value(shotOpt);
    // Give the scene a moment to lay out and paint before grabbing it.
    QTimer::singleShot(800, window, [window, path] {
      const QImage image = window->grabWindow();
      if (image.isNull() || !image.save(path)) {
        std::fprintf(stderr, "screenshot: could not save %s\n", qPrintable(path));
        QCoreApplication::exit(4);
        return;
      }
      QCoreApplication::exit(0);
    });
  }
  return qtApp.exec();
}
