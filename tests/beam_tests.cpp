#include <QtTest>
#include <QTemporaryDir>
#include <QProcess>
#include <QFile>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickStyle>
#include "backend.h"
#include "theme.h"
#include "translator.h"
class BeamTests : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QString video;
private slots:
    void initTestCase() {
        QQuickStyle::setStyle("Material");
        video = dir.filePath("Test screening.mp4");
        QProcess ff;
        ff.start("ffmpeg", {"-v","error","-f","lavfi","-i","testsrc2=size=320x180:rate=24","-t","3","-c:v","mpeg4","-y",video});
        QVERIFY(ff.waitForFinished(15000)); QCOMPARE(ff.exitCode(),0);
    }
    void queuePreservesActiveAndSession() {
        QString state = dir.filePath("queue.json");
        {
            Backend b(state,true);
            b.addFiles({QUrl::fromLocalFile(video),QUrl::fromLocalFile(video)});
            QCOMPARE(b.playlist().size(),2); QCOMPARE(b.current(),0);
            b.move(0,1); QCOMPARE(b.current(),1);
            b.remove(0); QCOMPARE(b.current(),0);
            b.setContinuous(true); b.setVolume(42);
        }
        Backend restored(state,true);
        QCOMPARE(restored.playlist().size(),1); QCOMPARE(restored.volume(),42);
        QVERIFY(restored.continuous());
        restored.addFiles({QUrl::fromLocalFile(dir.filePath("missing.mp4"))});
        QCOMPARE(restored.playlist().size(),1); QVERIFY(!restored.error().isEmpty());
    }
    void realMpvTransportAndStandby() {
        Backend b(dir.filePath("transport.json"),true);
        b.addFiles({QUrl::fromLocalFile(video),QUrl::fromLocalFile(video)});
        b.play();
        QTRY_VERIFY_WITH_TIMEOUT(b.ready(),6000);
        QTRY_VERIFY_WITH_TIMEOUT(b.duration()>2,5000);
        QTRY_VERIFY(b.position()>.1);
        b.togglePause(); QTRY_VERIFY(!b.playing());
        b.seek(1); QTRY_VERIFY(std::abs(b.position()-1)<.15);
        b.setVolume(36); b.setMuted(true); b.setSpeed(1.5);
        QTest::qWait(150); QCOMPARE(b.volume(),36); QVERIFY(b.muted()); QCOMPARE(b.speed(),1.5);
        b.stop(); QVERIFY(!b.mediaActive()); QVERIFY(b.projecting());
        const QString image = dir.filePath("standby.png");
        QImage img(320,180,QImage::Format_RGB32); img.fill(Qt::blue); QVERIFY(img.save(image));
        b.setPlaceholder(QUrl::fromLocalFile(image)); QTest::qWait(250);
        QVERIFY(!b.mediaActive()); QVERIFY(b.error().isEmpty());
        b.play(0); QTRY_VERIFY(b.duration()>2);
        b.seek(2.9); QTRY_VERIFY_WITH_TIMEOUT(!b.mediaActive(),4000);
        QCOMPARE(b.current(),0); QVERIFY(b.projecting());
        b.next(); QTRY_VERIFY(b.mediaActive()); QCOMPARE(b.current(),1);
        b.setContinuous(true); b.setRepeat(true);
        QTRY_VERIFY(b.duration()>2); b.seek(2.9);
        QTRY_COMPARE_WITH_TIMEOUT(b.current(),0,4000);
        b.endProjection(); QVERIFY(!b.projecting()); QVERIFY(!b.ready());
        b.project(); QTRY_VERIFY_WITH_TIMEOUT(b.ready(),6000); QVERIFY(!b.mediaActive());
        b.endProjection(); QVERIFY(b.error().isEmpty());
    }
    void themeAndLanguage() {
        Theme t(dir.path()); QCOMPARE(t.accent(),QString("#FFD60A"));
        QDir(dir.path()).mkpath("theme"); QFile colors(dir.filePath("theme/colors.toml"));
        QVERIFY(colors.open(QIODevice::WriteOnly)); colors.write("accent = \"#112233\"\n"); colors.close();
        Theme dark(dir.path()); QCOMPARE(dark.accent(),QString("#112233")); QCOMPARE(dark.accentForeground(),QString("white"));
        QFile lang(dir.filePath("language")); QVERIFY(lang.open(QIODevice::WriteOnly)); lang.write("da\n"); lang.close();
        QFile catalog(dir.filePath("da.json")); QVERIFY(catalog.open(QIODevice::WriteOnly)); catalog.write("{\"Playlist\":\"Afspilningsliste\"}"); catalog.close();
        Translator tr(dir.filePath("language"),dir.path()); QCOMPARE(tr.language(),QString("da"));
        QCOMPARE(tr.translate("Main","Playlist"),QString("Afspilningsliste"));
    }
    void qmlInteraction() {
        Backend b(dir.filePath("qml.json"),!qEnvironmentVariableIsSet("BEAM_LIVE_TEST")); Theme theme(dir.path()); Translator translator(dir.filePath("nonexistent"));
        b.addFiles({QUrl::fromLocalFile(video)});
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("backend",&b);
        engine.rootContext()->setContextProperty("theme",&theme);
        engine.rootContext()->setContextProperty("translator",&translator);
        engine.load(QUrl("qrc:/Main.qml")); QVERIFY(!engine.rootObjects().isEmpty());
        auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first()); QVERIFY(w);
        QTest::qWait(100); w->requestActivate(); QTest::qWait(100);
        if (qEnvironmentVariableIsSet("BEAM_LIVE_TEST")) w->grabWindow().save("/home/jeanc/Work/beam/.impeccable/review/live-test-desktop.png");
        QTest::keyClick(w,Qt::Key_Space); QTRY_VERIFY_WITH_TIMEOUT(b.ready(),6000);
        QTRY_VERIFY(b.mediaActive());
        QTest::keyClick(w,Qt::Key_S); QTRY_VERIFY(!b.mediaActive()); QVERIFY(b.projecting());
        w->resize(720,740); QTest::qWait(100);
        QVERIFY(!w->grabWindow().isNull());
        if (qEnvironmentVariableIsSet("BEAM_LIVE_TEST")) w->grabWindow().save("/home/jeanc/Work/beam/.impeccable/review/live-test-compact.png");
        b.endProjection();
    }
};
QTEST_MAIN(BeamTests)
#include "beam_tests.moc"
