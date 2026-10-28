#include "backend.h"
#include "mpris.h"
#include "theme.h"
#include "translator.h"
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
int main(int argc, char **argv) {
  const bool commandLine =
      argc > 1 && (QString::fromLocal8Bit(argv[1]) == "check" ||
                   QString::fromLocal8Bit(argv[1]) == "convert");
  if (commandLine) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QPA_PLATFORMTHEME", "");
  }
  QGuiApplication app(argc, argv);
  app.setApplicationName("beam");
  app.setApplicationVersion("0.2.0");
  app.setDesktopFileName("beam");
  app.setWindowIcon(QIcon::fromTheme("beam"));
  if (commandLine) {
    const auto arguments = app.arguments();
    const bool convert = arguments[1] == "convert";
    const int required = convert ? 4 : 3;
    if (arguments.size() < required) {
      QTextStream(stderr) << "Usage: beam check PLAYLIST [--json] | beam "
                             "convert INPUT OUTPUT [--json]\n";
      return 1;
    }
    QTemporaryDir temporary;
    Backend checker(temporary.filePath("session.json"), true);
    bool ok =
        checker.loadPlaylist(QFileInfo(arguments[2]).absoluteFilePath(), true);
    QStringList missing;
    if (ok)
      for (const auto &item : checker.playlist())
        if (item.toMap().value("missing").toBool())
          missing << item.toMap().value("path").toString();
    if (ok && convert) {
      const auto output = QFileInfo(arguments[3]).absoluteFilePath();
      ok = checker.savePlaylist(
          output, output.endsWith(".m3u", Qt::CaseInsensitive) ||
                      output.endsWith(".m3u8", Qt::CaseInsensitive));
    }
    const QString error = ok ? QString() : checker.error();
    if (!convert && !missing.isEmpty())
      ok = false;
    const QJsonObject result{{"ok", ok},
                             {"items", checker.count()},
                             {"missing", QJsonArray::fromStringList(missing)},
                             {"error", error}};
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact)
                        << "\n";
    if (!ok)
      QTextStream(stderr)
          << (error.isEmpty()
                  ? QString("Missing media: %1").arg(missing.join(", "))
                  : error)
          << "\n";
    return ok ? 0 : 1;
  }
  QQuickStyle::setStyle("Material");
  Translator translator(qEnvironmentVariable("BEAM_LANGUAGE_FILE"));
  app.installTranslator(&translator);
  Theme theme(qEnvironmentVariable("BEAM_THEME_DIRECTORY"));
  Backend backend(qEnvironmentVariable("BEAM_STATE_FILE"),
                  qEnvironmentVariableIsSet("BEAM_HEADLESS_MPV"));
  QList<QUrl> files;
  for (const auto &arg : app.arguments().mid(1))
    files << QUrl::fromLocalFile(QFileInfo(arg).absoluteFilePath());
  if (!files.isEmpty())
    backend.addFiles(files);
  Mpris mpris(&backend);
  QQmlApplicationEngine engine;
  QObject::connect(&translator, &Translator::changed, &engine,
                   &QQmlEngine::retranslate);
  engine.rootContext()->setContextProperty("translator", &translator);
  engine.rootContext()->setContextProperty("theme", &theme);
  engine.rootContext()->setContextProperty("backend", &backend);
  engine.load(QUrl("qrc:/Main.qml"));
  if (engine.rootObjects().isEmpty())
    return 1;
  QObject::connect(&mpris, &Mpris::raiseRequested, &engine, [&engine] {
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    window->show();
    window->raise();
    window->requestActivate();
  });
  if (qEnvironmentVariableIsSet("BEAM_CAPTURE_COMPACT")) {
    auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    w->resize(620, 540);
  }
  if (qEnvironmentVariableIsSet("BEAM_CAPTURE_PREVIEW")) {
    backend.setPreviewEnabled(true);
    qobject_cast<QQuickWindow *>(engine.rootObjects().first())
        ->resize(800, 740);
  }
  if (qEnvironmentVariableIsSet("BEAM_CAPTURE_SETTINGS"))
    QTimer::singleShot(200, &app, [&] {
      auto *drawer =
          engine.rootObjects().first()->findChild<QObject *>("settings");
      if (drawer)
        QMetaObject::invokeMethod(drawer, "open");
    });
  const auto capture = qEnvironmentVariable("BEAM_CAPTURE");
  if (!capture.isEmpty()) {
    backend.updateScreens({"DEMO-HDMI-1"},
                          {"DEMO-HDMI-1 · Demo display · 1920×1080"});
    backend.setScreen("DEMO-HDMI-1");
  }
  if (!capture.isEmpty())
    QTimer::singleShot(1200, &app, [&] {
      auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
      w->grabWindow().save(capture);
      w->setProperty("allowClose", true);
      app.quit();
    });
  return app.exec();
}
