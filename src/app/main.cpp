#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <qqml.h>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <cstdio>
#include <memory>

#include "app_state.h"
#include "audio_controller.h"
#include "demo_source.h"
#include "frame_item.h"
#include "frame_store.h"
#include "meeting_controller.h"
#include "settings_saver.h"

int main(int argc, char* argv[]) {
  QGuiApplication qtApp(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("backhaul"));
  QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
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
                                   QStringLiteral("Start on a settings page: zoom, slots, audio or video."), QStringLiteral("page"));
  args.addOption(pageOpt);
  const QCommandLineOption roleOpt(QStringLiteral("role"),
                                   QStringLiteral("Start in a role: all, camera, twoshot or audio; \"ask\" shows the role question."),
                                   QStringLiteral("role"));
  args.addOption(roleOpt);
  args.process(qtApp);
  if (args.isSet(roleOpt)) {
    const QString role = args.value(roleOpt);
    if (role != QStringLiteral("all") && role != QStringLiteral("camera") && role != QStringLiteral("twoshot") &&
        role != QStringLiteral("audio") && role != QStringLiteral("ask")) {
      std::fprintf(stderr, "--role wants all, camera, twoshot, audio or ask\n");
      return 5;
    }
  }

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
                          ? QStringLiteral("設定ファイルを読めませんでした。初期設定で動いています（保存はしません）")
                          : QStringLiteral("設定ファイルを読めなかったため、初期設定に戻しました（元のファイルは残してあります）");
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

  AudioController audio(state, demo ? nullptr : backhaul::makeAudioOutput(),
                        demo ? nullptr : backhaul::makeAsioOutput(), demo ? nullptr : backhaul::makeAudioInput());
  // `meeting` is created before `audio` and so outlives it and its sending thread.
  audio.setSender([&meeting](const std::int16_t* samples, std::size_t frames, int channels) {
    return meeting.sendAudio(samples, frames, channels);
  });
  if (!demo) audio.startSaved();
  meeting.setMixer(audio.mixer());

  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty(QStringLiteral("meeting"), &meeting);
  engine.rootContext()->setContextProperty(QStringLiteral("audio"), &audio);
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
        QString title = s->name().isEmpty() ? QStringLiteral("モニター%1").arg(number) : s->name();
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
        page != QStringLiteral("zoom")) {
      std::fprintf(stderr, "--settings wants zoom, slots, audio or video\n");
      return 5;
    }
    window->setProperty("settingsTab", page);
    window->setProperty("settingsOpen", true);
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
