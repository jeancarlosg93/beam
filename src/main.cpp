#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QIcon>
#include "backend.h"
#include "theme.h"
#include "translator.h"
int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("beam"); app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("beam"); app.setWindowIcon(QIcon::fromTheme("beam"));
    QQuickStyle::setStyle("Material");
    Translator translator(qEnvironmentVariable("BEAM_LANGUAGE_FILE")); app.installTranslator(&translator);
    Theme theme(qEnvironmentVariable("BEAM_THEME_DIRECTORY"));
    Backend backend(qEnvironmentVariable("BEAM_STATE_FILE"), qEnvironmentVariableIsSet("BEAM_HEADLESS_MPV"));
    QList<QUrl> files;
    for (const auto &arg : app.arguments().mid(1)) files << QUrl::fromLocalFile(QFileInfo(arg).absoluteFilePath());
    if (!files.isEmpty()) backend.addFiles(files);
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    if (qEnvironmentVariableIsSet("BEAM_CAPTURE_COMPACT")) {
        auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        w->resize(720,740);
    }
    const auto capture = qEnvironmentVariable("BEAM_CAPTURE");
    if (!capture.isEmpty()) QTimer::singleShot(1200, &app, [&] {
        auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        w->grabWindow().save(capture); app.quit();
    });
    return app.exec();
}
