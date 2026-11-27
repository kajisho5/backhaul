#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>

#include "app_state.h"
#include "demo_source.h"

int main(int argc, char* argv[]) {
  QGuiApplication qtApp(argc, argv);
  QGuiApplication::setApplicationName(QStringLiteral("backhaul"));
  QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));

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
  args.process(qtApp);

  AppState state;
  if (args.isSet(demoOpt)) loadDemo(state);

  QQmlApplicationEngine engine;
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
