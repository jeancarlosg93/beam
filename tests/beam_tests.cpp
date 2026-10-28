#include "backend.h"
#include "mpris.h"
#include "theme.h"
#include "translator.h"
#include <QFile>
#include <QElapsedTimer>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>
class BeamTests : public QObject {
  Q_OBJECT
  QTemporaryDir dir;
  QString video, multi;
  QQuickItem *findItem(QQuickItem *parent, const QString &name) {
    if (parent->objectName() == name)
      return parent;
    for (auto *item : parent->childItems())
      if (auto *found = findItem(item, name))
        return found;
    return nullptr;
  }
  QJsonValue property(Backend &b, const QString &name) {
    b.m_socket.flush();
    QCoreApplication::processEvents();
    QLocalSocket client;
    client.connectToServer(b.m_runtime.path() + "/mpv.sock");
    if (!client.waitForConnected(1000))
      return {};
    client.write(QJsonDocument(
                     QJsonObject{{"command", QJsonArray{"get_property", name}},
                                 {"request_id", 1}})
                     .toJson(QJsonDocument::Compact) +
                 '\n');
    client.flush();
    QByteArray response;
    QElapsedTimer deadline;
    deadline.start();
    while (deadline.elapsed() < 1000) {
      response += client.readAll();
      while (response.contains('\n')) {
        const int end = response.indexOf('\n');
        const auto message = QJsonDocument::fromJson(response.left(end)).object();
        response.remove(0, end + 1);
        // mpv events can arrive before the reply on a newly connected socket.
        if (message["request_id"].toInt() == 1)
          return message["data"];
      }
      client.waitForReadyRead(qMax(1, 1000 - int(deadline.elapsed())));
    }
    return {};
  }
private slots:
  void initTestCase() {
    QQuickStyle::setStyle("Material");
    video = dir.filePath("Test screening.mp4");
    QProcess ff;
    ff.start("ffmpeg", {"-v", "error", "-f", "lavfi", "-i",
                        "testsrc2=size=320x180:rate=24", "-t", "3", "-c:v",
                        "mpeg4", "-y", video});
    QVERIFY(ff.waitForFinished(15000));
    QCOMPARE(ff.exitCode(), 0);
    const QString subtitle = dir.filePath("captions.srt");
    QFile sub(subtitle);
    QVERIFY(sub.open(QIODevice::WriteOnly));
    sub.write("1\n00:00:00,000 --> 00:00:05,000\nSynthetic caption\n");
    sub.close();
    multi = dir.filePath("Multitrack étude.mkv");
    ff.start("ffmpeg", {"-v",
                        "error",
                        "-f",
                        "lavfi",
                        "-i",
                        "testsrc2=size=320x180:rate=24",
                        "-f",
                        "lavfi",
                        "-i",
                        "sine=frequency=440",
                        "-f",
                        "lavfi",
                        "-i",
                        "sine=frequency=660",
                        "-i",
                        subtitle,
                        "-map",
                        "0:v",
                        "-map",
                        "1:a",
                        "-map",
                        "2:a",
                        "-map",
                        "3:s",
                        "-metadata:s:a:0",
                        "language=eng",
                        "-metadata:s:a:1",
                        "language=dan",
                        "-metadata:s:a:1",
                        "title=Danish demo",
                        "-metadata:s:s:0",
                        "language=eng",
                        "-t",
                        "6",
                        "-c:v",
                        "mpeg4",
                        "-c:a",
                        "aac",
                        "-c:s",
                        "srt",
                        "-y",
                        multi});
    QVERIFY(ff.waitForFinished(15000));
    QCOMPARE(ff.exitCode(), 0);
  }
  void queuePreservesActiveAndSession() {
    QString state = dir.filePath("queue.json");
    {
      Backend b(state, true);
      b.addFiles({QUrl::fromLocalFile(video), QUrl::fromLocalFile(video)});
      QCOMPARE(b.playlist().size(), 2);
      QCOMPARE(b.current(), 0);
      b.move(0, 1);
      QCOMPARE(b.current(), 1);
      b.remove(0);
      QCOMPARE(b.current(), 0);
      b.setContinuous(true);
      b.setVolume(42);
    }
    Backend restored(state, true);
    QCOMPARE(restored.playlist().size(), 1);
    QCOMPARE(restored.volume(), 42);
    QVERIFY(restored.continuous());
    restored.addFiles({QUrl::fromLocalFile(dir.filePath("missing.mp4"))});
    QCOMPARE(restored.playlist().size(), 1);
    QVERIFY(!restored.error().isEmpty());
  }
  void realMpvTransportAndStandby() {
    Backend b(dir.filePath("transport.json"), true);
    b.addFiles({QUrl::fromLocalFile(video), QUrl::fromLocalFile(video)});
    b.play();
    QTRY_VERIFY_WITH_TIMEOUT(b.ready(), 6000);
    QTRY_VERIFY_WITH_TIMEOUT(b.duration() > 2, 5000);
    QTRY_VERIFY(b.position() > .1);
    b.togglePause();
    QTRY_VERIFY(!b.playing());
    b.seek(1);
    QTRY_VERIFY(std::abs(b.position() - 1) < .15);
    b.setVolume(36);
    b.setMuted(true);
    b.setSpeed(1.5);
    QTest::qWait(150);
    QCOMPARE(b.volume(), 36);
    QVERIFY(b.muted());
    QCOMPARE(b.speed(), 1.5);
    b.stop();
    QVERIFY(!b.mediaActive());
    QVERIFY(b.projecting());
    const QString image = dir.filePath("standby.png");
    QImage img(320, 180, QImage::Format_RGB32);
    img.fill(Qt::blue);
    QVERIFY(img.save(image));
    b.setPlaceholder(QUrl::fromLocalFile(image));
    QTest::qWait(250);
    QVERIFY(!b.mediaActive());
    QVERIFY(b.error().isEmpty());
    b.play(0);
    QTRY_VERIFY(b.duration() > 2);
    b.seek(2.9);
    QTRY_VERIFY_WITH_TIMEOUT(!b.mediaActive(), 4000);
    QCOMPARE(b.current(), 0);
    QVERIFY(b.projecting());
    b.next();
    QTRY_VERIFY(b.mediaActive());
    QCOMPARE(b.current(), 1);
    b.setContinuous(true);
    b.setRepeat(true);
    QTRY_VERIFY(b.duration() > 2);
    b.seek(2.9);
    QTRY_COMPARE_WITH_TIMEOUT(b.current(), 0, 4000);
    b.endProjection();
    QVERIFY(!b.projecting());
    QVERIFY(!b.ready());
    b.project();
    QTRY_VERIFY_WITH_TIMEOUT(b.ready(), 6000);
    QVERIFY(!b.mediaActive());
    b.endProjection();
    QVERIFY(b.error().isEmpty());
  }
  void mprisSystemControls() {
    Backend b(dir.filePath("mpris.json"), true);
    b.addFiles({QUrl::fromLocalFile(video), QUrl::fromLocalFile(video)});
    const QString service = "org.mpris.MediaPlayer2.beam.test" +
                            QString::number(QCoreApplication::applicationPid());
    Mpris mpris(&b, service);
    QVERIFY(mpris.registered());
    auto call = [&](QStringList arguments, QString *output = nullptr) {
      QProcess client;
      client.start("gdbus", QStringList{"call", "--session", "--dest", service,
                                        "--object-path",
                                        "/org/mpris/MediaPlayer2", "--method"} +
                                arguments);
      QElapsedTimer timer;
      timer.start();
      while (client.state() != QProcess::NotRunning && timer.elapsed() < 5000)
        QTest::qWait(10);
      if (output)
        *output = QString::fromUtf8(client.readAllStandardOutput());
      if (client.state() != QProcess::NotRunning) {
        client.kill();
        client.waitForFinished();
        return false;
      }
      if (client.exitCode() != 0)
        qWarning().noquote() << client.readAllStandardError();
      return client.exitCode() == 0;
    };
    const QString iface = "org.mpris.MediaPlayer2.Player";
    QString result;
    QVERIFY(
        call({"org.freedesktop.DBus.Properties.Get", iface, "PlaybackStatus"},
             &result));
    QVERIFY(result.contains("Stopped"));
    QVERIFY(call({iface + ".PlayPause"}));
    QTRY_VERIFY_WITH_TIMEOUT(b.ready() && b.duration() > 2, 6000);
    QVERIFY(call({iface + ".Pause"}));
    QTRY_VERIFY(!b.playing());
    QVERIFY(
        call({"org.freedesktop.DBus.Properties.Get", iface, "PlaybackStatus"},
             &result));
    QVERIFY(result.contains("Paused"));
    QVERIFY(call({iface + ".Next"}));
    QTRY_COMPARE(b.current(), 1);
    QTRY_VERIFY(b.duration() > 2);
    QTRY_VERIFY(!b.playing());
    for (int attempt = 0; attempt < 6; ++attempt) {
      QVERIFY(call({iface + (attempt % 2 ? ".Next" : ".Previous")}));
      QTRY_COMPARE(b.current(), attempt % 2 ? 1 : 0);
      QTRY_VERIFY(b.duration() > 2);
      QTRY_VERIFY(!b.playing());
      QTest::qWait(120);
      QVERIFY(b.mediaActive());
      QVERIFY(!b.playing());
    }
    QVERIFY(call({iface + ".Play"}));
    QTRY_VERIFY(b.playing());
    QVERIFY(call({iface + ".Pause"}));
    QTRY_VERIFY(!b.playing());
    QVERIFY(call(
        {"org.freedesktop.DBus.Properties.Set", iface, "Volume", "<0.42>"}));
    QTRY_COMPARE(b.volume(), 42);
    QVERIFY(call({iface + ".Seek", "1000000"}));
    QTRY_VERIFY(b.position() > .9);
    QVERIFY(call({"org.freedesktop.DBus.Properties.GetAll", iface}, &result));
    QVERIFY(result.contains("xesam:title"));
    QVERIFY(result.contains("mpris:trackid"));
    QVERIFY(call({iface + ".Stop"}));
    QVERIFY(!b.mediaActive());
    QVERIFY(b.projecting());
    QVERIFY(call({iface + ".Previous"}));
    QCOMPARE(b.current(), 0);
    QVERIFY(!b.mediaActive());
    b.endProjection();
  }
  void privateCueAndPresentation() {
    Backend b(dir.filePath("cue.json"), true);
    b.addFiles({QUrl::fromLocalFile(multi), QUrl::fromLocalFile(multi)});
    const QString first = b.mediaId(0), second = b.mediaId(1);
    QVERIFY(first != second);
    QTRY_VERIFY(b.playlist()[1].toMap()["tracks"].toList().size() == 3);
    b.play(0, true);
    QTRY_VERIFY_WITH_TIMEOUT(b.duration() > 5, 6000);
    QTRY_VERIFY(!b.playing());
    b.togglePause();
    QTRY_VERIFY(b.playing());
    const double beforeCue = b.position();
    b.cue(1);
    b.setCueSetting("start", 1.25);
    QTRY_VERIFY(b.position() > beforeCue + .1);
    QCOMPARE(b.current(), 0);
    b.pause();
    QTRY_VERIFY(!b.playing());
    b.setCueSetting("audio", "2");
    b.setCueSetting("subtitle", "1");
    b.setCueSetting("aspect", "16:9");
    b.setCueSetting("volume", 27);
    QCOMPARE(b.current(), 0);
    QVERIFY(b.mediaActive());
    QVERIFY(!b.playing());
    QCOMPARE(property(b, "aid").toInt(), 1);
    QCOMPARE(property(b, "volume").toInt(), 80);
    QVERIFY(b.position() < 1.);
    b.playCue();
    QTRY_COMPARE(b.current(), 1);
    QTRY_VERIFY(b.duration() > 5);
    QTRY_VERIFY(b.position() > 1.2);
    QCOMPARE(property(b, "aid").toInt(), 2);
    QCOMPARE(property(b, "sid").toInt(), 1);
    QCOMPARE(property(b, "volume").toInt(), 27);
    QVERIFY(std::abs(property(b, "video-aspect-override").toDouble() -
                     16. / 9) < .01);
    b.move(0, 1);
    QCOMPARE(b.mediaId(b.current()), second);
    QCOMPARE(b.mediaId(b.cueIndex()), second);
    b.remove(1);
    QCOMPARE(b.mediaId(b.current()), second);
    QVERIFY(b.mediaActive());
    b.resetCueSettings();
    QCOMPARE(property(b, "volume").toInt(),
             27); // Edits remain private even for the live occurrence.
    b.playCue();
    QTRY_VERIFY(b.position() < 1);
    QTRY_COMPARE(property(b, "volume").toInt(), 80);
    QTRY_COMPARE(property(b, "aid").toInt(), 1);
    QVERIFY(!property(b, "sid").toBool());
    b.setCueSetting("audio", "99");
    b.setCueSetting("subtitle", "99");
    b.playCue();
    QTRY_VERIFY(b.duration() > 5);
    QTRY_COMPARE(property(b, "aid").toInt(), 1);
    QTRY_VERIFY(!property(b, "sid").toBool());
    b.endProjection();
  }
  void reusablePlaylists() {
    const QString named = dir.filePath("Show étude.beam"),
                  m3u = dir.filePath("Show étude.m3u8");
    QString id;
    {
      Backend b(dir.filePath("named-session.json"), true);
      b.addFiles({QUrl::fromLocalFile(multi), QUrl::fromLocalFile(multi)});
      b.cue(1);
      b.setCueSetting("start", 1.5);
      b.setCueSetting("volume", 33);
      b.setCueSetting("audio", "2");
      b.setCueSetting("subtitle", "1");
      b.setCueSetting("aspect", "fill");
      id = b.mediaId(1);
      QVERIFY(b.savePlaylist(named));
      QVERIFY(!b.dirty());
      QVERIFY(b.savePlaylist(m3u, true));
      b.setCueSetting("start", 2);
      QVERIFY(!b.loadPlaylist(named));
      QVERIFY(b.loadPlaylist(named, true));
      QCOMPARE(b.mediaId(1), id);
      b.cue(1);
      QCOMPARE(b.cueItem()["start"].toDouble(), 1.5);
      QCOMPARE(b.cueItem()["volume"].toInt(), 33);
      QCOMPARE(b.cueItem()["audio"].toString(), QString("2"));
      QCOMPARE(b.cueItem()["aspect"].toString(), QString("fill"));
      QVERIFY(b.loadPlaylist(m3u, true));
      QCOMPARE(b.count(), 2);
      QCOMPARE(b.mediaPath(0), multi);
      QCOMPARE(b.mediaPath(1), multi);
      QVERIFY(b.mediaId(0) != b.mediaId(1));
      QVERIFY(!b.savePlaylist(dir.filePath("no-folder/show.beam")));
      QFile remote(dir.filePath("remote.m3u"));
      QVERIFY(remote.open(QIODevice::WriteOnly));
      remote.write("#EXTM3U\nhttps://example.org/movie.mp4\n");
      remote.close();
      QVERIFY(!b.loadPlaylist(remote.fileName(), true));
      QCOMPARE(b.count(), 2);
      QFile missing(dir.filePath("missing.m3u"));
      QVERIFY(missing.open(QIODevice::WriteOnly));
      missing.write("#EXTM3U\n./missing file.mp4\n");
      missing.close();
      QVERIFY(b.loadPlaylist(missing.fileName(), true));
      QVERIFY(b.playlist()[0].toMap()["missing"].toBool());
      QVERIFY(b.totalDuration().contains("unknown"));
      b.playCue();
      QVERIFY(!b.projecting());
      b.relinkCue(QUrl::fromLocalFile(multi));
      QVERIFY(!b.cueItem()["missing"].toBool());
    }
    QFile legacy(dir.filePath("legacy.json"));
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    legacy.write(QJsonDocument(QJsonObject{{"files", QJsonArray{video, video}},
                                           {"volume", 45}})
                     .toJson());
    legacy.close();
    Backend old(legacy.fileName(), true);
    QCOMPARE(old.count(), 2);
    QCOMPARE(old.volume(), 45);
    QVERIFY(!old.mediaId(0).isEmpty());
    QVERIFY(old.mediaId(0) != old.mediaId(1));
    old.setVolume(46);
    QFile migrated(legacy.fileName());
    QVERIFY(migrated.open(QIODevice::ReadOnly));
    QCOMPARE(
        QJsonDocument::fromJson(migrated.readAll()).object()["version"].toInt(),
        2);
  }
  void blackoutAndAudioDevices() {
    Backend b(dir.filePath("blackout.json"), true);
    b.addFiles({QUrl::fromLocalFile(multi), QUrl::fromLocalFile(multi)});
    b.setAudioDevices(
        {QVariantMap{{"id", "null"}, {"label", "Synthetic null output"}}});
    b.setAudioDevice("null");
    QVERIFY(b.audioDeviceAvailable());
    b.play(0, true);
    QTRY_VERIFY_WITH_TIMEOUT(b.duration() > 5, 6000);
    QTRY_VERIFY(!b.playing());
    b.setBlackoutAudio(false);
    b.setBlackout(true);
    QTRY_VERIFY(property(b, "mute").toBool());
    QTRY_VERIFY(!property(b, "vf").toArray().isEmpty());
    QTRY_VERIFY(!property(b, "sub-visibility").toBool());
    const double position = b.position();
    QTest::qWait(200);
    QVERIFY(std::abs(b.position() - position) < .1);
    QVERIFY(!b.playing());
    b.setVolume(38);
    b.setMuted(true);
    b.setBlackout(false);
    QTRY_VERIFY(property(b, "mute").toBool());
    QTRY_COMPARE(property(b, "volume").toInt(), 38);
    b.setMuted(false);
    b.togglePause();
    QTRY_VERIFY(b.playing());
    b.setBlackoutAudio(true);
    b.setBlackout(true);
    QTRY_VERIFY(!property(b, "mute").toBool());
    const double playingPosition = b.position();
    QTRY_VERIFY(b.position() > playingPosition + .15);
    b.next();
    QTRY_COMPARE(b.current(), 1);
    QTRY_VERIFY(b.duration() > 5);
    QVERIFY(b.blackout());
    QTRY_VERIFY(!property(b, "vf").toArray().isEmpty());
    QTRY_VERIFY(!property(b, "sub-visibility").toBool());
    b.seek(5.9);
    QTRY_VERIFY_WITH_TIMEOUT(!b.mediaActive(), 4000);
    QVERIFY(b.blackout());
    b.stop();
    QVERIFY(b.blackout());
    b.endProjection();
    QVERIFY(!b.blackout());
    b.setAudioDevices({});
    QVERIFY(!b.audioDeviceAvailable());
    QCOMPARE(b.audioDevice(), QString("null"));
    b.setAudioDevice("auto");
    QVERIFY(b.audioDeviceAvailable());
    b.setAudioDevice("missing");
    QCOMPARE(b.audioDevice(), QString("auto"));
  }
  void previewModesPersist() {
    const QString state = dir.filePath("preview-mode.json");
    Backend b(state, true);
    QCOMPARE(b.previewMode(), QString("low"));
    QCOMPARE(b.m_previewTimer.interval(), 700);
    b.setPreviewMode("smooth");
    QCOMPARE(b.m_previewTimer.interval(), 84);
    QVERIFY(!b.m_previewTimer.isActive());
    Backend restored(state, true);
    QCOMPARE(restored.previewMode(), QString("smooth"));
    QCOMPARE(restored.m_previewTimer.interval(), 84);
    QVERIFY(!restored.previewEnabled());
    QVERIFY(!restored.projecting());
    b.setPreviewMode("invalid");
    QCOMPARE(b.previewMode(), QString("smooth"));
    b.setPreviewMode("low");
    QCOMPARE(b.m_previewTimer.interval(), 700);
    Backend low(state, true);
    QCOMPARE(low.previewMode(), QString("low"));
  }
  void previewsAndDisplayRecovery() {
    Backend b(dir.filePath("preview.json"), true);
    b.addFiles({QUrl::fromLocalFile(multi)});
    QVERIFY(!b.m_previewTimer.isActive());
    b.setPreviewEnabled(true);
    QTRY_VERIFY_WITH_TIMEOUT(!b.cuePreview().isEmpty(), 6000);
    const QString originalPreview = b.cuePreview();
    b.relinkCue(QUrl::fromLocalFile(video));
    QVERIFY(b.cuePreview().isEmpty());
    QTRY_VERIFY_WITH_TIMEOUT(!b.cuePreview().isEmpty(), 6000);
    QVERIFY(b.cuePreview() != originalPreview);
    b.remove(0);
    QVERIFY(b.cuePreview().isEmpty());
    QTest::qWait(250);
    QVERIFY(b.cuePreview().isEmpty());
    b.addFiles({QUrl::fromLocalFile(multi)});
    QTRY_VERIFY_WITH_TIMEOUT(!b.cuePreview().isEmpty(), 6000);
    b.play(0, true);
    QTRY_VERIFY_WITH_TIMEOUT(b.duration() > 5, 6000);
    QTest::qWait(1000);
    QVERIFY(!b.livePreview().isEmpty() ||
            b.previewMessage() == b.tr("Output frame unavailable"));
    b.setPreviewMode("smooth");
    const int lowCaptures = b.m_captureSequence;
    QTRY_VERIFY_WITH_TIMEOUT(b.m_captureSequence >= lowCaptures + 4, 2000);
    QVERIFY(!b.playing());
    const QString pending = b.m_capturePath;
    b.m_capturePath = "capture-in-progress";
    const int beforeBlocked = b.m_captureSequence;
    b.capturePreview();
    b.capturePreview();
    QCOMPARE(b.m_captureSequence, beforeBlocked);
    b.m_capturePath = pending;
    b.setPreviewEnabled(false);
    const int captures = b.m_captureSequence;
    QTest::qWait(850);
    QCOMPARE(b.m_captureSequence, captures);
    QVERIFY(!b.m_previewTimer.isActive());
    QCOMPARE(b.m_cueProcess.state(), QProcess::NotRunning);
    QVERIFY(b.livePreview().isEmpty());
    QVERIFY(b.cuePreview().isEmpty());
    b.updateScreens({"DEMO-1", "DEMO-2"}, {"Demo one", "Demo two"});
    b.endProjection();
    b.setScreen("DEMO-2");
    b.project();
    QTRY_VERIFY(b.ready());
    b.testPattern();
    QVERIFY(b.testing());
    b.stop();
    QVERIFY(!b.testing());
    b.updateScreens({"DEMO-1"}, {"Demo one"});
    QVERIFY(!b.projecting());
    QCOMPARE(b.screen(), QString("DEMO-2"));
    b.updateScreens({"DEMO-1", "DEMO-2"}, {"Demo one", "Demo two"});
    QCOMPARE(b.screen(), QString("DEMO-2"));
    QVERIFY(!b.projecting());
    b.identifyDisplays();
    QCOMPARE(b.m_identify.size(), 2);
    for (int i = 0; i < b.m_identify.size(); ++i) {
      auto *process = b.m_identify[i];
      QVERIFY(process->waitForStarted(3000));
      QVERIFY(process->arguments().contains("--fs-screen-name=DEMO-" + QString::number(i + 1)));
      QVERIFY(process->arguments().contains("--screen-name=DEMO-" + QString::number(i + 1)));
    }
    QVERIFY(b.m_identifyTimer.isActive());
    b.dismissIdentify();
    QVERIFY(b.m_identify.isEmpty());
    QVERIFY(!b.m_identifyTimer.isActive());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }
  void themeAndLanguage() {
    Theme t(dir.path());
    QCOMPARE(t.accent(), QString("#FFD60A"));
    QDir(dir.path()).mkpath("theme");
    QFile colors(dir.filePath("theme/colors.toml"));
    QVERIFY(colors.open(QIODevice::WriteOnly));
    colors.write("accent = \"#112233\"\n");
    colors.close();
    Theme dark(dir.path());
    QCOMPARE(dark.accent(), QString("#112233"));
    QCOMPARE(dark.accentForeground(), QString("white"));
    QCOMPARE(dark.colors().value("foreground").toString(), QString("#eeeeef"));
    QVERIFY(colors.open(QIODevice::WriteOnly | QIODevice::Truncate));
    colors.write(
        "mode = \"light\"\naccent = \"#123456\"\nbackground = "
        "\"#eff1f5\"\nforeground = \"#4c4f69\"\nselection = \"#ccd0da\"\n");
    colors.close();
    QTRY_VERIFY(dark.light());
    QCOMPARE(dark.colors().value("background").toString(), QString("#eff1f5"));
    QCOMPARE(dark.colors().value("selection").toString(), QString("#ccd0da"));
    QCOMPARE(dark.accent(), QString("#123456"));
    QFile shell(dir.filePath("theme/shell.toml"));
    QVERIFY(shell.open(QIODevice::WriteOnly));
    shell.write("[font]\nbase-size = 13\n");
    shell.close();
    QTRY_COMPARE(dark.fontSize(), 13);
    QFile lang(dir.filePath("language"));
    QVERIFY(lang.open(QIODevice::WriteOnly));
    lang.write("da\n");
    lang.close();
    QFile catalog(dir.filePath("da.json"));
    QVERIFY(catalog.open(QIODevice::WriteOnly));
    catalog.write("{\"Playlist\":\"Afspilningsliste\"}");
    catalog.close();
    Translator tr(dir.filePath("language"), dir.path());
    QCOMPARE(tr.language(), QString("da"));
    QCOMPARE(tr.translate("Main", "Playlist"), QString("Afspilningsliste"));
  }
  void qmlInteraction() {
    Backend b(dir.filePath("qml.json"),
              !qEnvironmentVariableIsSet("BEAM_LIVE_TEST"));
    Theme theme(dir.path());
    Translator translator(dir.filePath("nonexistent"));
    b.addFiles({QUrl::fromLocalFile(video), QUrl::fromLocalFile(video)});
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &b);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.load(QUrl("qrc:/Main.qml"));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    QVERIFY(w);
    QTest::qWait(100);
    w->requestActivate();
    QTest::qWait(100);
    if (qEnvironmentVariableIsSet("BEAM_LIVE_TEST"))
      w->grabWindow().save(
          "/home/jeanc/Work/beam/.impeccable/review/live-test-desktop.png");
    QTest::keyClick(w, Qt::Key_Space);
    QTRY_VERIFY_WITH_TIMEOUT(b.ready(), 6000);
    QTRY_VERIFY(b.mediaActive());
    QTest::keyClick(w, Qt::Key_S);
    QTRY_VERIFY(!b.mediaActive());
    QVERIFY(b.projecting());
    w->resize(620, 540);
    QTest::qWait(100);
    QVERIFY(!w->grabWindow().isNull());
    if (qEnvironmentVariableIsSet("BEAM_LIVE_TEST"))
      w->grabWindow().save(
          "/home/jeanc/Work/beam/.impeccable/review/live-test-compact.png");
    b.endProjection();
    auto *cueRow = findItem(w->contentItem(), "cueRow1");
    QVERIFY(cueRow);
    const QPoint cuePoint =
        cueRow->mapToScene(QPointF(cueRow->width() / 2, cueRow->height() / 2))
            .toPoint();
    QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier, cuePoint);
    QTRY_COMPARE(b.cueIndex(), 1);
    const QString second = b.mediaId(1);
    auto *handle = findItem(w->contentItem(), "reorder1");
    QVERIFY(handle);
    const QPoint source = handle->mapToScene(QPointF(10, 20)).toPoint();
    QTest::mousePress(w, Qt::LeftButton, Qt::NoModifier, source);
    QTest::mouseMove(w, source - QPoint(0, 44), 100);
    QTest::mouseRelease(w, Qt::LeftButton, Qt::NoModifier,
                        source - QPoint(0, 44));
    QTRY_COMPARE(b.mediaId(0), second);
    QCOMPARE(b.cueIndex(), 0);
    auto *settings = w->findChild<QObject *>("settings");
    QVERIFY(settings);
    QVERIFY(QMetaObject::invokeMethod(settings, "open"));
    QTest::qWait(200);
    auto *scroll = settings->findChild<QQuickItem *>("settingsScroll");
    QVERIFY(scroll);
    QVERIFY(scroll->width() > 300);
    QVERIFY(scroll->height() > 450);
    QVERIFY(QMetaObject::invokeMethod(settings, "close"));
    QTest::qWait(200);
    b.setPreviewEnabled(true);
    w->resize(800, 740);
    auto *previewMode = findItem(w->contentItem(), "previewMode");
    QVERIFY(previewMode);
    previewMode->forceActiveFocus();
    QTest::keyClick(w, Qt::Key_Space);
    QTest::keyClick(w, Qt::Key_End);
    QTest::keyClick(w, Qt::Key_Return);
    QTRY_COMPARE(b.previewMode(), QString("smooth"));
    QVERIFY(b.dirty());
    QVERIFY(!w->close());
    auto *confirmation = w->findChild<QObject *>("quitConfirm");
    QVERIFY(confirmation);
    QTRY_VERIFY(confirmation->property("opened").toBool());
    auto *quitButton = findItem(w->contentItem(), "quitWithoutSaving");
    QVERIFY(quitButton);
    const QPoint quitPoint = quitButton->mapToScene(
        QPointF(quitButton->width() / 2, quitButton->height() / 2)).toPoint();
    QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier, quitPoint);
    QTRY_VERIFY(!w->isVisible());
    QVERIFY(w->property("allowClose").toBool());
  }
};
QTEST_MAIN(BeamTests)
#include "beam_tests.moc"
