#include "backend.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QStandardPaths>
#include <algorithm>
#include <cmath>

Backend::Backend(const QString &stateFile, bool headless, QObject *parent)
    : QObject(parent), m_queue(this), m_stateFile(stateFile),
      m_headless(headless) {
  if (m_stateFile.isEmpty())
    m_stateFile =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        "/session.json";
  QFile file(m_stateFile);
  if (file.open(QIODevice::ReadOnly)) {
    const auto o = QJsonDocument::fromJson(file.readAll()).object();
    QJsonArray items = o["items"].toArray();
    if (items.isEmpty())
      for (const auto &p : o["files"].toArray())
        items << QJsonObject{{"path", p.toString()}};
    m_queue.load(items);
    m_outputMode =
        o["outputMode"].toString() == "chromecast" ? "chromecast" : "local";
    m_receiver = o["receiver"].toString();
    m_placeholder = o["placeholder"].toString();
    m_screen = o["screen"].toString();
    m_volume = qBound(0, o["volume"].toInt(80), 100);
    m_continuous = o["continuous"].toBool();
    m_repeat = o["repeat"].toBool();
    m_audioDevice = o["audioDevice"].toString("auto");
    m_blackoutAudio = o["blackoutAudio"].toBool(true);
    m_previewMode = o["previewMode"].toString() == "smooth" ? "smooth" : "low";
    m_playlistFile = o["playlistFile"].toString();
    m_dirty = o["dirty"].toBool();
    m_current = count() ? 0 : -1;
    m_cue = m_queue.index(o["cueId"].toString());
    if (m_cue < 0 && count())
      m_cue = 0;
  }
  connect(&m_queue, &MediaQueue::updated, this, [this] {
    emit playlistChanged();
    emit changed();
  });
  connect(&m_queue, &MediaQueue::failed, this, &Backend::fail);
  connect(&m_cast, &OutputDriver::failed, this, &Backend::fail);
  connect(&m_cast, &OutputDriver::devicesFound, this,
          [this](const QVariantList &devices) {
            m_receivers = devices;
            emit changed();
          });
  connect(&m_cast, &OutputDriver::stateChanged, this, [this] {
    if (localOutput())
      return;
    if (!m_cast.active()) {
      m_media = false;
      m_pendingId.clear();
      m_position = m_duration = 0;
      m_remoteState.clear();
    } else if (ready() && !m_pendingId.isEmpty()) {
      const int i = m_queue.index(m_pendingId);
      m_pendingId.clear();
      if (i >= 0)
        play(i, m_pendingPaused);
    }
    emit changed();
  });
  connect(&m_cast, &OutputDriver::status, this, [this](const QJsonObject &o) {
    if (localOutput() || !m_media || o["id"].toString() != m_remoteId)
      return;
    const auto state = o["state"].toString();
    m_remoteState = state;
    m_position = o["position"].toDouble();
    m_duration = o["duration"].toDouble();
    if (state == "finished") {
      if (m_continuous)
        next();
      else
        standby();
    } else if (state == "stopped" || state == "error")
      standby();
    else {
      m_paused = state != "PLAYING";
      if (state == "PAUSED" || state == "PLAYING")
        m_desiredPaused = m_paused;
    }
    emit changed();
  });
  refreshScreens();
  connect(qApp, &QGuiApplication::screenAdded, this,
          [this] { refreshScreens(); });
  connect(qApp, &QGuiApplication::screenRemoved, this,
          [this] { refreshScreens(); });
  connect(&m_connectTimer, &QTimer::timeout, this, [this] {
    if (++m_retries > 60) {
      endProjection();
      fail(tr("mpv did not respond. End projection and try again."));
      return;
    }
    m_socket.abort();
    m_socket.connectToServer(m_runtime.path() + "/mpv.sock");
  });
  m_connectTimer.setInterval(100);
  connect(&m_socket, &QLocalSocket::connected, this, [this] {
    m_connectTimer.stop();
    int id = 0;
    for (const QString p : {"pause", "time-pos", "duration", "speed"})
      command({"observe_property", ++id, p});
    applyAudio();
    setSpeed(m_speed);
    if (!m_pendingId.isEmpty()) {
      const int i = m_queue.index(m_pendingId);
      m_pendingId.clear();
      if (i >= 0)
        play(i, m_pendingPaused);
      else
        standby();
    } else
      standby();
    emit changed();
  });
  connect(&m_socket, &QLocalSocket::readyRead, this, &Backend::readMessages);
  connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
    m_log += m_process.readAllStandardError();
    m_log = m_log.right(3000);
  });
  connect(
      &m_process, &QProcess::errorOccurred, this,
      [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
          m_connectTimer.stop();
          fail(tr("Cannot start mpv. Install the mpv package and try again."));
        }
      });
  connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int code, QProcess::ExitStatus status) {
            m_connectTimer.stop();
            m_socket.abort();
            m_buffer.clear();
            m_requests.clear();
            m_capturePath.clear();
            m_media = false;
            m_pendingId.clear();
            m_position = m_duration = 0;
            m_blackout = false;
            m_testing = false;
            invalidatePreview();
            if (code != 0 || status == QProcess::CrashExit)
              fail(tr("mpv exited unexpectedly: %1")
                       .arg(QString::fromUtf8(m_log).trimmed()));
            emit changed();
          });
  m_identifyTimer.setSingleShot(true);
  m_identifyTimer.setInterval(5000);
  connect(&m_identifyTimer, &QTimer::timeout, this, &Backend::dismissIdentify);
  m_previewTimer.setTimerType(Qt::PreciseTimer);
  m_previewTimer.setInterval(m_previewMode == "smooth" ? 84 : 700);
  connect(&m_previewTimer, &QTimer::timeout, this, &Backend::capturePreview);
  m_cueTimer.setSingleShot(true);
  m_cueTimer.setInterval(180);
  connect(&m_cueTimer, &QTimer::timeout, this, &Backend::captureCue);
  connect(&m_cueProcess,
          qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this](int code, QProcess::ExitStatus) {
            if (m_previewEnabled && code == 0 &&
                m_cueToken ==
                    mediaId(m_cue) +
                        QString::number(cueItem().value("start").toDouble()) &&
                QFileInfo::exists(m_cuePath))
              m_cuePreview = QUrl::fromLocalFile(m_cuePath).toString();
            emit previewChanged();
          });
  connect(&m_deviceProcess,
          qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this](int, QProcess::ExitStatus) {
            const QString output =
                QString::fromUtf8(m_deviceProcess.readAllStandardOutput());
            QVariantList devices;
            const QRegularExpression re("^\\s*'([^']+)'\\s*\\((.*)\\)\\s*$");
            for (const auto &line : output.split('\n')) {
              const auto m = re.match(line);
              if (m.hasMatch() && m.captured(1) != "auto")
                devices << QVariantMap{{"id", m.captured(1)},
                                       {"label", m.captured(2)}};
            }
            setAudioDevices(devices);
          });
  connect(&m_deviceProcess, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart)
              fail(tr("Cannot enumerate audio devices. Install mpv and refresh "
                      "devices."));
          });
  m_audioDevices = {
      QVariantMap{{"id", "auto"}, {"label", tr("System default")}}};
  refreshAudioDevices();
}
Backend::~Backend() {
  dismissIdentify();
  setPreviewEnabled(false);
  endProjection();
  disconnect(&m_cast, nullptr, this, nullptr);
  m_cast.close();
  if (m_deviceProcess.state() != QProcess::NotRunning) {
    m_deviceProcess.kill();
    m_deviceProcess.waitForFinished(1000);
  }
}
void Backend::fail(const QString &s) {
  m_error = s;
  emit changed();
}
QString Backend::playlistName() const {
  return m_playlistFile.isEmpty() ? tr("Unsaved playlist")
                                  : QFileInfo(m_playlistFile).fileName();
}
QString Backend::outputState() const {
  if (!projecting())
    return tr("Projection off");
  if (!ready())
    return tr("Connecting…");
  if (!localOutput()) {
    if (!m_media)
      return tr("Connected · No video");
    if (m_remoteState == "loading" || m_remoteState == "BUFFERING")
      return tr("Loading video…");
  }
  if (m_blackout)
    return tr("BLACKOUT");
  if (m_testing)
    return tr("Test pattern");
  if (!m_media)
    return tr("Standby");
  return playing() ? tr("Playing") : tr("Paused");
}
void Backend::refreshScreens() {
  QStringList names, labels;
  for (auto *s : QGuiApplication::screens()) {
    names << s->name();
    labels << QString("%1 · %2 %3 · %4×%5")
                  .arg(s->name(), s->manufacturer(), s->model())
                  .arg(s->size().width())
                  .arg(s->size().height());
  }
  updateScreens(names, labels);
}
void Backend::updateScreens(const QStringList &names,
                            const QStringList &labels) {
  m_screens = names;
  m_screenLabels = labels;
  if (m_screen.isEmpty())
    m_screen = names.size() > 1 ? names.last() : names.value(0);
  if (localOutput() && !names.contains(m_screen) && projecting()) {
    endProjection();
    fail(tr("Output %1 disconnected. Reconnect it or explicitly choose another "
            "display.")
             .arg(m_screen));
  }
  emit changed();
}
void Backend::save() {
  QDir().mkpath(QFileInfo(m_stateFile).absolutePath());
  QSaveFile f(m_stateFile);
  const QJsonObject o{{"version", 2},
                      {"items", m_queue.json()},
                      {"cueId", mediaId(m_cue)},
                      {"placeholder", m_placeholder},
                      {"screen", m_screen},
                      {"outputMode", m_outputMode},
                      {"receiver", m_receiver},
                      {"volume", m_volume},
                      {"continuous", m_continuous},
                      {"repeat", m_repeat},
                      {"audioDevice", m_audioDevice},
                      {"blackoutAudio", m_blackoutAudio},
                      {"previewMode", m_previewMode},
                      {"playlistFile", m_playlistFile},
                      {"dirty", m_dirty}};
  if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(o).toJson()) < 0 ||
      !f.commit())
    fail(tr("Could not save the playlist session. Check available disk space "
            "and permissions."));
}
void Backend::edited() {
  m_dirty = true;
  save();
  emit playlistChanged();
  emit changed();
}
void Backend::setOutputMode(const QString &mode) {
  if ((mode != "local" && mode != "chromecast") || projecting() ||
      discovering() || mode == m_outputMode)
    return;
  setPreviewEnabled(false);
  dismissIdentify();
  m_outputMode = mode;
  m_error.clear();
  save();
  emit changed();
}
void Backend::setReceiver(const QString &id) {
  if (projecting() || discovering())
    return;
  for (const auto &device : m_receivers) {
    if (device.toMap()["id"].toString() == id) {
      m_receiver = id;
      save();
      emit changed();
      return;
    }
  }
}
void Backend::discoverReceivers() {
  if (!localOutput() && !projecting()) {
    m_error.clear();
    m_cast.discover();
  }
}
void Backend::setScreen(const QString &s) {
  if (projecting() || !m_screens.contains(s))
    return;
  m_screen = s;
  save();
  emit changed();
}
void Backend::addFiles(const QList<QUrl> &urls) {
  for (const auto &u : urls) {
    const QString p = u.toLocalFile();
    if (p.isEmpty() || !QFileInfo(p).isFile() || !QFileInfo(p).isReadable()) {
      fail(tr("Cannot open %1. Choose a readable local media file.")
               .arg(u.toDisplayString()));
      continue;
    }
    m_queue.add(QFileInfo(p).absoluteFilePath());
  }
  if (m_current < 0 && count())
    m_current = 0;
  if (m_cue < 0 && count()) {
    m_cue = 0;
    resetCuePreview();
  }
  edited();
}
void Backend::command(const QJsonArray &args, const QString &purpose) {
  if (!localOutput() || !ready())
    return;
  QJsonObject o{{"command", args}};
  if (!purpose.isEmpty()) {
    const int id = ++m_requestId;
    o["request_id"] = id;
    m_requests[id] = purpose;
  }
  m_socket.write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n');
}
void Backend::project() {
  if (projecting())
    return;
  if (!localOutput()) {
    if (discovering())
      return;
    if (m_receiver.isEmpty()) {
      fail(tr("Search for and choose a Chromecast receiver first."));
      return;
    }
    m_error.clear();
    m_cast.open(m_receiver);
    return;
  }
  if (!m_headless && !m_screens.contains(m_screen)) {
    fail(tr("The selected output is unavailable. Choose a connected display."));
    return;
  }
  m_error.clear();
  m_log.clear();
  m_retries = 0;
  m_buffer.clear();
  m_requests.clear();
  QStringList args{"--no-config",
                   "--idle=yes",
                   "--keep-open=no",
                   "--terminal=no",
                   "--osc=no",
                   "--osd-level=0",
                   "--input-default-bindings=no",
                   "--input-vo-keyboard=no",
                   "--cursor-autohide=always",
                   "--stop-screensaver=yes",
                   "--image-display-duration=inf",
                   "--audio-client-name=Beam",
                   "--screenshot-format=jpg",
                   "--screenshot-jpeg-quality=75",
                   "--input-ipc-server=" + m_runtime.path() + "/mpv.sock"};
  if (m_headless)
    args << "--vo=null" << "--ao=null";
  else
    args << "--force-window=immediate" << "--fullscreen=yes"
         << "--screen-name=" + m_screen << "--fs-screen-name=" + m_screen
         << "--wayland-app-id=beam-output" << "--title=Beam output";
  m_process.start("mpv", args);
  m_connectTimer.start();
  emit changed();
}
void Backend::endProjection() {
  if (!localOutput()) {
    m_pendingId.clear();
    m_cast.close();
    m_media = false;
    m_position = m_duration = 0;
    emit changed();
    return;
  }
  m_connectTimer.stop();
  m_pendingId.clear();
  if (m_process.state() != QProcess::NotRunning) {
    command({"quit"});
    m_socket.flush();
    if (!m_process.waitForFinished(1500)) {
      m_process.terminate();
      if (!m_process.waitForFinished(500)) {
        m_process.kill();
        m_process.waitForFinished(500);
      }
    }
  }
  m_socket.abort();
  m_capturePath.clear();
  m_requests.clear();
  m_media = false;
  m_blackout = false;
  m_testing = false;
  invalidatePreview();
  emit changed();
}
void Backend::play(int i, bool paused) {
  if (i < 0)
    i = m_media ? m_current : m_cue;
  if (i < 0 || i >= count())
    return;
  if (!QFileInfo(mediaPath(i)).isReadable()) {
    fail(tr("File is missing or unreadable: %1. Relink or remove it.")
             .arg(mediaPath(i)));
    return;
  }
  if (!localOutput() && (discovering() || m_receiver.isEmpty())) {
    fail(tr("Search for and choose a Chromecast receiver first."));
    return;
  }
  if (!ready()) {
    m_pendingId = mediaId(i);
    m_pendingPaused = paused;
    project();
    return;
  }
  m_current = i;
  m_media = true;
  m_testing = false;
  m_paused = m_desiredPaused = paused;
  m_position = m_duration = 0;
  invalidatePreview();
  if (!localOutput()) {
    m_liveItem = m_queue.items[i];
    m_remoteState = "loading";
    m_remoteId = m_liveItem.id + ":" + QString::number(++m_remoteSequence);
    applyAudio();
    m_cast.send({{"op", "play"},
                 {"id", m_remoteId},
                 {"path", m_liveItem.path},
                 {"title", QFileInfo(m_liveItem.path).fileName()},
                 {"start", m_liveItem.start},
                 {"paused", paused}});
    emit changed();
    return;
  }
  command({"set_property", "pause", paused});
  // Per-load options are applied atomically before the first decoded frame.
  m_liveItem = m_queue.items[i];
  const auto &item = m_liveItem;
  QJsonObject options{{"start", QString::number(item.start)},
                      {"aid", item.audio},
                      {"sid", item.subtitle},
                      {"video-aspect-override", item.aspect == "4:3"    ? "4:3"
                                                : item.aspect == "16:9" ? "16:9"
                                                                        : "-1"},
                      {"panscan", item.aspect == "fill" ? "1" : "0"}};
  options["sub-visibility"] = m_blackout ? "no" : "yes";
  options["vf"] = m_blackout ? "lavfi=[drawbox=c=black:t=fill]" : "";
  command({"loadfile", item.path, "replace", -1, options});
  applyAudio();
  command({"set_property", "speed", m_speed});
  emit changed();
}
void Backend::applyItem() {
  if (!m_media || m_current < 0)
    return;
  const auto &i = m_liveItem;
  command({"set_property", "aid", i.audio});
  command({"set_property", "sid", i.subtitle});
  command({"set_property", "video-aspect-override",
           i.aspect == "4:3"    ? "4:3"
           : i.aspect == "16:9" ? "16:9"
                                : "-1"});
  command({"set_property", "panscan", i.aspect == "fill" ? 1 : 0});
  applyAudio();
}
void Backend::standby() {
  m_media = false;
  m_testing = false;
  m_paused = m_desiredPaused = true;
  m_position = m_duration = 0;
  invalidatePreview();
  if (!localOutput()) {
    if (ready())
      m_cast.send({{"op", "stop"}});
    m_remoteState.clear();
    applyAudio();
    emit changed();
    return;
  }
  command({"stop"});
  if (!m_placeholder.isEmpty()) {
    if (QFileInfo::exists(m_placeholder)) {
      command({"loadfile", m_placeholder, "replace", -1,
               QJsonObject{{"vf", m_blackout ? "lavfi=[drawbox=c=black:t=fill]"
                                             : ""}}});
      command({"set_property", "pause", false});
    } else
      fail(tr("Standby image is missing. Showing black instead."));
  }
  emit changed();
}
void Backend::stop() {
  m_pendingId.clear();
  standby();
}
void Backend::togglePause() {
  if (!m_media) {
    play();
    return;
  }
  m_desiredPaused = !m_desiredPaused;
  if (localOutput())
    command({"set_property", "pause", m_desiredPaused});
  else
    m_cast.send({{"op", "pause"}, {"paused", m_desiredPaused}});
}
void Backend::pause() {
  if (!m_media)
    return;
  m_desiredPaused = true;
  if (localOutput())
    command({"set_property", "pause", true});
  else
    m_cast.send({{"op", "pause"}, {"paused", true}});
}
void Backend::next() {
  if (!count())
    return;
  if (m_current + 1 < count())
    play(m_current + 1);
  else if (m_repeat)
    play(0);
  else
    stop();
}
void Backend::previous() {
  if (m_current >= 0)
    play(qMax(0, m_current - 1));
}
void Backend::seek(double s) {
  if (!m_media)
    return;
  const double target = qBound(0., s, m_duration);
  if (localOutput())
    command({"seek", target, "absolute+exact"});
  else
    m_cast.send({{"op", "seek"}, {"position", target}, {"paused", m_desiredPaused}});
  emit seeked(target);
}
void Backend::applyAudio() {
  const int override = m_media && m_current >= 0 ? m_liveItem.volume : -1;
  if (!localOutput()) {
    if (ready())
      m_cast.send({{"op", "volume"},
                   {"volume", override < 0 ? m_volume : override},
                   {"muted", m_muted}});
    return;
  }
  command({"set_property", "volume", override < 0 ? m_volume : override});
  command(
      {"set_property", "mute", m_muted || (m_blackout && !m_blackoutAudio)});
  command({"set_property", "audio-device",
           audioDeviceAvailable() ? m_audioDevice : "auto"},
          "audio:" + m_audioDevice);
}
void Backend::setVolume(int v) {
  m_volume = qBound(0, v, 100);
  applyAudio();
  save();
  emit changed();
}
void Backend::setMuted(bool v) {
  m_muted = v;
  applyAudio();
  emit changed();
}
void Backend::setSpeed(double v) {
  if (!localOutput())
    return;
  m_speed = qBound(.25, v, 3.);
  command({"set_property", "speed", m_speed});
  emit changed();
}
void Backend::setContinuous(bool v) {
  m_continuous = v;
  save();
  emit changed();
}
void Backend::setRepeat(bool v) {
  m_repeat = v;
  save();
  emit changed();
}
void Backend::cue(int i) {
  if (i < 0 || i >= count())
    return;
  m_cue = i;
  if (!m_media)
    m_current = i;
  resetCuePreview();
  save();
  emit playlistChanged();
  emit changed();
  emit previewChanged();
}
void Backend::setCueSetting(const QString &key, const QVariant &v) {
  if (m_cue < 0 || m_cue >= count())
    return;
  auto &i = m_queue.items[m_cue];
  if (key == "start") {
    i.start = qMax(0., v.toDouble());
    if (i.duration >= 0)
      i.start = qMin(i.start, qMax(0., i.duration - .05));
    resetCuePreview();
  } else if (key == "volume")
    i.volume = qBound(-1, v.toInt(), 100);
  else if (key == "audio" || key == "subtitle") {
    const QString value = v.toString();
    if (value != "auto" && value != "no" &&
        !QRegularExpression("^[1-9][0-9]*$").match(value).hasMatch())
      return;
    if (key == "audio")
      i.audio = value;
    else
      i.subtitle = value;
  } else if (key == "aspect") {
    if (!QStringList{"preserve", "fill", "4:3", "16:9"}.contains(v.toString()))
      return;
    i.aspect = v.toString();
  } else
    return;
  // Cue edits are always private; they take effect only on the next explicit
  // Play cue.
  edited();
  emit previewChanged();
}
void Backend::resetCueSettings() {
  if (m_cue < 0)
    return;
  auto &i = m_queue.items[m_cue];
  i.start = 0;
  i.volume = -1;
  i.audio = "auto";
  i.subtitle = "no";
  i.aspect = "preserve";
  edited();
  resetCuePreview();
}
void Backend::relinkCue(const QUrl &url) {
  if (m_cue < 0 || !url.isLocalFile() ||
      !QFileInfo(url.toLocalFile()).isReadable()) {
    fail(tr("Choose a readable local replacement file."));
    return;
  }
  if (m_media && m_cue == m_current) {
    fail(tr("Stop the live item before relinking it."));
    return;
  }
  auto &i = m_queue.items[m_cue];
  i.path = url.toLocalFile();
  i.duration = -1;
  m_queue.probe(i.id);
  resetCuePreview();
  edited();
}
void Backend::move(int from, int to) {
  if (from < 0 || to < 0 || from >= count() || to >= count() || from == to)
    return;
  const QString active = mediaId(m_current), cueId = mediaId(m_cue);
  m_queue.items.move(from, to);
  m_current = m_queue.index(active);
  m_cue = m_queue.index(cueId);
  edited();
}
void Backend::remove(int i) {
  if (i < 0 || i >= count())
    return;
  if (i == m_current && m_media)
    stop();
  const QString active = mediaId(m_current), cueId = mediaId(m_cue);
  m_queue.items.removeAt(i);
  m_current = m_queue.index(active);
  m_cue = m_queue.index(cueId);
  if (m_current < 0 && count())
    m_current = qMin(i, count() - 1);
  if (m_cue < 0 && count())
    m_cue = qMin(i, count() - 1);
  if (cueId != mediaId(m_cue))
    resetCuePreview();
  edited();
}
void Backend::shuffle() {
  const QString active = mediaId(m_current), cueId = mediaId(m_cue);
  std::shuffle(m_queue.items.begin(), m_queue.items.end(),
               *QRandomGenerator::global());
  m_current = m_queue.index(active);
  m_cue = m_queue.index(cueId);
  edited();
}
QString Backend::clock(double seconds) const {
  if (seconds < 0)
    return tr("Unknown");
  const int s = qMax(0, int(seconds));
  return s >= 3600
             ? QString("%1:%2:%3")
                   .arg(s / 3600)
                   .arg(s / 60 % 60, 2, 10, QChar('0'))
                   .arg(s % 60, 2, 10, QChar('0'))
             : QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}
void Backend::setPlaceholder(const QUrl &url) {
  const auto p = url.toLocalFile();
  if (!QFileInfo(p).isFile() || !QImageReader(p).canRead()) {
    fail(tr("Choose a readable local image file (PNG, JPEG or WebP)."));
    return;
  }
  m_placeholder = p;
  save();
  if (ready() && !m_media)
    standby();
  emit changed();
}
void Backend::clearPlaceholder() {
  m_placeholder.clear();
  save();
  if (ready() && !m_media)
    standby();
  emit changed();
}
void Backend::setBlackout(bool v) {
  if (!localOutput() || !projecting() || m_blackout == v)
    return;
  m_blackout = v;
  command({"set_property", "sub-visibility", !v});
  command({"set_property", "vf", v ? "lavfi=[drawbox=c=black:t=fill]" : ""},
          "blackout");
  applyAudio();
  invalidatePreview();
  emit changed();
}
void Backend::setBlackoutAudio(bool v) {
  m_blackoutAudio = v;
  applyAudio();
  save();
  emit changed();
}
bool Backend::audioDeviceAvailable() const {
  for (const auto &v : m_audioDevices)
    if (v.toMap()["id"].toString() == m_audioDevice)
      return true;
  return m_audioDevice == "auto";
}
void Backend::setAudioDevices(const QVariantList &devices) {
  m_audioDevices = {
      QVariantMap{{"id", "auto"}, {"label", tr("System default")}}};
  for (const auto &v : devices)
    if (v.toMap()["id"].toString() != "auto")
      m_audioDevices << v;
  if (!audioDeviceAvailable()) {
    m_deviceWarning =
        tr("Saved audio output %1 is unavailable. Using System default until "
           "it returns or you select another device.")
            .arg(m_audioDevice);
    fail(m_deviceWarning);
  }
  if (audioDeviceAvailable() && m_error == m_deviceWarning)
    m_error.clear();
  if (ready())
    applyAudio();
  emit changed();
}
void Backend::refreshAudioDevices() {
  if (m_deviceProcess.state() != QProcess::NotRunning)
    return;
  m_deviceProcess.start("mpv", {"--no-config", "--audio-device=help"});
}
void Backend::setAudioDevice(const QString &v) {
  bool found = false;
  for (const auto &d : m_audioDevices)
    if (d.toMap()["id"].toString() == v)
      found = true;
  if (!found) {
    fail(tr("That audio device is unavailable. Choose System default or "
            "reconnect it."));
    return;
  }
  m_audioDevice = v;
  applyAudio();
  save();
  emit changed();
}
void Backend::invalidatePreview() {
  ++m_previewGeneration;
  if (!m_livePreview.isEmpty())
    QFile::remove(QUrl(m_livePreview).toLocalFile());
  m_livePreview.clear();
  m_previewMessage = tr("Loading output frame…");
  emit previewChanged();
}
void Backend::setPreviewEnabled(bool v) {
  if (v && !localOutput())
    return;
  if (m_previewEnabled == v)
    return;
  m_previewEnabled = v;
  if (v) {
    m_previewTimer.start();
    capturePreview();
    m_cueTimer.start();
  } else {
    m_previewTimer.stop();
    m_cueTimer.stop();
    if (m_cueProcess.state() != QProcess::NotRunning) {
      m_cueProcess.kill();
      m_cueProcess.waitForFinished(1000);
    }
    invalidatePreview();
    m_cuePreview.clear();
  }
  emit changed();
  emit previewChanged();
}
void Backend::setPreviewMode(const QString &value) {
  if ((value != "low" && value != "smooth") || m_previewMode == value)
    return;
  m_previewMode = value;
  // Keep at most one capture in flight even if the selected cadence is faster
  // than the output device can capture. Never queue up work behind playback.
  m_previewTimer.setInterval(value == "smooth" ? 84 : 700);
  save();
  emit changed();
}
void Backend::capturePreview() {
  if (!m_previewEnabled)
    return;
  if (!projecting() || !ready() || m_blackout || (!m_media && !m_testing)) {
    invalidatePreview();
    return;
  }
  if (!m_capturePath.isEmpty())
    return;
  m_capturePath =
      m_runtime.filePath(QString("live-%1.jpg").arg(++m_captureSequence));
  command({"screenshot-to-file", m_capturePath, "window"},
          "preview:" + QString::number(m_previewGeneration) + ":" +
              m_capturePath);
}
void Backend::resetCuePreview() {
  // Reject an in-flight frame before changing the cue's identity or contents.
  m_cueToken.clear();
  m_cuePreview.clear();
  m_cueTimer.stop();
  if (m_cueProcess.state() != QProcess::NotRunning)
    m_cueProcess.kill();
  if (m_previewEnabled && m_cue >= 0)
    m_cueTimer.start();
  emit previewChanged();
}
void Backend::captureCue() {
  if (!m_previewEnabled || m_cue < 0)
    return;
  if (m_cueProcess.state() != QProcess::NotRunning) {
    m_cueProcess.kill();
    m_cueProcess.waitForFinished(1000);
  }
  m_cueToken =
      mediaId(m_cue) + QString::number(cueItem().value("start").toDouble());
  if (!m_cuePath.isEmpty())
    QFile::remove(m_cuePath);
  m_cuePreview.clear();
  m_cuePath =
      m_runtime.filePath(QString("cue-%1.png").arg(++m_captureSequence));
  m_cueProcess.start("ffmpeg",
                     {"-v", "error", "-nostdin", "-ss",
                      QString::number(cueItem().value("start").toDouble()),
                      "-i", mediaPath(m_cue), "-frames:v", "1", "-vf",
                      "scale=480:-1", "-an", "-y", m_cuePath});
  const QString token = m_cueToken;
  QTimer::singleShot(10000, this, [this, token] {
    if (token == m_cueToken && m_cueProcess.state() != QProcess::NotRunning)
      m_cueProcess.kill();
  });
  emit previewChanged();
}
void Backend::readMessages() {
  m_buffer += m_socket.readAll();
  while (m_buffer.contains('\n')) {
    const int end = m_buffer.indexOf('\n');
    const auto o = QJsonDocument::fromJson(m_buffer.left(end)).object();
    m_buffer.remove(0, end + 1);
    const int request = o["request_id"].toInt();
    const QString purpose = m_requests.take(request);
    const QString err = o["error"].toString();
    if (purpose.startsWith("preview:")) {
      const int split = purpose.indexOf(':', 8);
      const int generation = purpose.mid(8, split - 8).toInt();
      const QString path = purpose.mid(split + 1);
      m_capturePath.clear();
      if (m_previewEnabled && generation == m_previewGeneration &&
          err == "success" && QFileInfo::exists(path) &&
          (m_media || m_testing)) {
        if (!m_livePreview.isEmpty())
          QFile::remove(QUrl(m_livePreview).toLocalFile());
        m_livePreview = QUrl::fromLocalFile(path).toString();
        m_previewMessage.clear();
      } else {
        QFile::remove(path);
        m_livePreview.clear();
        m_previewMessage = tr("Output frame unavailable");
      }
      emit previewChanged();
      continue;
    }
    if (purpose.startsWith("loaded:")) {
      if (m_media && purpose.mid(7) == mediaId(m_current) &&
          o["data"].toString() == mediaPath(m_current)) {
        command({"set_property", "pause", m_desiredPaused});
        applyItem();
        command({"get_property", "track-list"}, "tracks:" + mediaId(m_current));
      }
      continue;
    }
    if (purpose.startsWith("tracks:")) {
      if (m_media && purpose.mid(7) == mediaId(m_current)) {
        const auto &item = m_liveItem;
        bool aid = item.audio == "auto" || item.audio == "no",
             sid = item.subtitle == "auto" || item.subtitle == "no";
        for (const auto &t : o["data"].toArray()) {
          const auto track = t.toObject();
          if (track["type"] == "audio" &&
              QString::number(track["id"].toInt()) == item.audio)
            aid = true;
          if (track["type"] == "sub" &&
              QString::number(track["id"].toInt()) == item.subtitle)
            sid = true;
        }
        if (!aid)
          command({"set_property", "aid", "auto"});
        if (!sid)
          command({"set_property", "sid", "no"});
      }
      continue;
    }
    const QString event = o["event"].toString();
    if (event == "property-change") {
      const auto name = o["name"].toString();
      const auto v = o["data"];
      if (name == "pause")
        m_paused = v.toBool();
      if (name == "time-pos" && m_media)
        m_position = v.toDouble();
      if (name == "duration" && m_media)
        m_duration = v.toDouble();
      if (name == "speed")
        m_speed = v.toDouble(1);
      emit changed();
    } else if (event == "file-loaded" && m_media)
      command({"get_property", "path"}, "loaded:" + mediaId(m_current));
    else if (event == "end-file" && m_media) {
      const auto reason = o["reason"].toString();
      if (reason == "eof") {
        if (m_continuous)
          next();
        else
          standby();
      } else if (reason == "error") {
        standby();
        fail(tr("mpv could not play this file: %1")
                 .arg(o["file_error"].toString()));
      }
    } else if (!err.isEmpty() && err != "success") {
      if (purpose.startsWith("audio:") && purpose.mid(6) != m_audioDevice)
        continue;
      if (purpose == "blackout") {
        m_blackout = false;
        applyAudio();
      }
      fail(
          purpose.startsWith("audio:")
              ? tr("mpv could not apply the selected audio device: %1").arg(err)
              : tr("mpv: %1").arg(err));
    }
  }
}
bool Backend::savePlaylist(const QString &path, bool m3u) {
  if (path.isEmpty())
    return false;
  QSaveFile f(path);
  QByteArray bytes;
  if (m3u) {
    bytes = "#EXTM3U\n";
    const QDir base(QFileInfo(path).absolutePath());
    for (const auto &i : m_queue.items) {
      const QString relative = base.relativeFilePath(i.path);
      if (relative.contains('\n') || relative.contains('\r')) {
        fail(tr("M3U cannot represent filenames containing line breaks. Save a "
                "Beam playlist instead."));
        return false;
      }
      bytes += ("./" + relative + "\n").toUtf8();
    }
  } else
    bytes = QJsonDocument(QJsonObject{{"format", "beam-playlist"},
                                      {"version", 2},
                                      {"items", m_queue.json()}})
                .toJson();
  if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() ||
      !f.commit()) {
    fail(tr("Cannot save %1. Check the folder and permissions.").arg(path));
    return false;
  }
  if (!m3u) {
    m_playlistFile = QFileInfo(path).absoluteFilePath();
    m_dirty = false;
    save();
    emit changed();
  }
  return true;
}
bool Backend::loadPlaylist(const QString &path, bool discard) {
  if (m_dirty && !discard) {
    fail(tr("Save the current playlist or confirm discarding changes before "
            "opening another."));
    return false;
  }
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly)) {
    fail(tr("Cannot read playlist: %1").arg(path));
    return false;
  }
  const auto bytes = f.readAll();
  QJsonArray items;
  const QDir base(QFileInfo(path).absolutePath());
  const bool m3u = path.endsWith(".m3u", Qt::CaseInsensitive) ||
                   path.endsWith(".m3u8", Qt::CaseInsensitive);
  if (m3u) {
    QString text = QString::fromUtf8(bytes);
    if (text.startsWith(QChar(0xfeff)))
      text.remove(0, 1);
    for (QString line : text.split('\n')) {
      if (line.endsWith('\r'))
        line.chop(1);
      if (line.isEmpty() || line.startsWith('#'))
        continue;
      const QUrl url(line);
      if (!url.scheme().isEmpty()) {
        fail(tr("Only local paths are supported in M3U playlists."));
        return false;
      }
      items << QJsonObject{
          {"path", QDir::cleanPath(base.absoluteFilePath(line))}};
    }
  } else {
    QJsonParseError error;
    const auto o = QJsonDocument::fromJson(bytes, &error).object();
    if (error.error != QJsonParseError::NoError ||
        o["format"] != "beam-playlist" || o["version"].toInt() != 2 ||
        !o["items"].isArray()) {
      fail(tr("This is not a supported Beam playlist (version 2)."));
      return false;
    }
    items = o["items"].toArray();
    for (const auto &v : items) {
      const auto p = v.toObject()["path"].toString();
      if (p.isEmpty() || !QDir::isAbsolutePath(p) ||
          !QUrl(p).scheme().isEmpty()) {
        fail(tr("Beam playlists require absolute local paths."));
        return false;
      }
    }
  }
  if (m_media || m_testing)
    stop();
  m_queue.load(items);
  m_current = m_cue = count() ? 0 : -1;
  m_playlistFile = m3u ? QString() : QFileInfo(path).absoluteFilePath();
  m_dirty = m3u;
  save();
  emit playlistChanged();
  emit changed();
  resetCuePreview();
  return true;
}
void Backend::chooseFiles(bool placeholder) {
  openPicker(placeholder ? "image" : "media");
}
void Backend::choosePlaylist(const QString &action) { openPicker(action); }
void Backend::openPicker(const QString &action) {
  if (m_pickerPending)
    return;
  m_pickerPending = true;
  m_pickerAction = action;
  const QString token =
      "beam" + QString::number(QRandomGenerator::global()->generate());
  QString sender = QDBusConnection::sessionBus().baseService().mid(1);
  sender.replace('.', '_');
  const QString path =
      "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
  auto bus = QDBusConnection::sessionBus();
  bus.connect("org.freedesktop.portal.Desktop", path,
              "org.freedesktop.portal.Request", "Response", this,
              SLOT(portalResponse(uint, QVariantMap)));
  QDBusInterface portal("org.freedesktop.portal.Desktop",
                        "/org/freedesktop/portal/desktop",
                        "org.freedesktop.portal.FileChooser", bus);
  const bool saving = action == "save" || action == "export";
  QVariantMap options{{"handle_token", token},
                      {"multiple", action == "media"},
                      {"modal", false}};
  if (saving)
    options["current_name"] =
        action == "save" ? "Screening.beam" : "Screening.m3u8";
  const auto call =
      portal.asyncCall(saving ? "SaveFile" : "OpenFile", QString(),
                       saving               ? tr("Save playlist")
                       : action == "image"  ? tr("Choose standby image")
                       : action == "media"  ? tr("Add media")
                       : action == "relink" ? tr("Relink cue")
                                            : tr("Open playlist"),
                       options);
  auto *watcher = new QDBusPendingCallWatcher(call, this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher](QDBusPendingCallWatcher *) {
            QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            if (reply.isError()) {
              m_pickerPending = false;
              fail(tr("Cannot open the system file picker: %1")
                       .arg(reply.error().message()));
            }
            watcher->deleteLater();
          });
}
void Backend::portalResponse(uint response, const QVariantMap &results) {
  m_pickerPending = false;
  if (response != 0)
    return;
  const QStringList uris = qdbus_cast<QStringList>(results.value("uris"));
  QList<QUrl> urls;
  for (const auto &s : uris)
    urls << QUrl(s);
  if (urls.isEmpty())
    return;
  const auto url = urls.first();
  if (!url.isLocalFile()) {
    fail(tr("Choose a local file."));
    return;
  }
  if (m_pickerAction == "image")
    setPlaceholder(url);
  else if (m_pickerAction == "media")
    addFiles(urls);
  else if (m_pickerAction == "relink")
    relinkCue(url);
  else if (m_pickerAction == "save" || m_pickerAction == "export")
    savePlaylist(url.toLocalFile(), m_pickerAction == "export");
  else
    loadPlaylist(url.toLocalFile(), m_pickerAction == "open-discard");
}
void Backend::identifyDisplays() {
  if (!localOutput())
    return;
  dismissIdentify();
  const QString inputPath = m_runtime.filePath("identify-input.conf");
  QFile input(inputPath);
  if (!input.open(QIODevice::WriteOnly) ||
      input.write("ESC quit\nMBTN_LEFT quit\n") < 0) {
    fail(tr("Cannot create the test pattern."));
    return;
  }
  input.close();
  for (int n = 0; n < m_screens.size(); ++n) {
    const QString connector = m_screens[n];
    QString manufacturer, model;
    for (auto *screen : QGuiApplication::screens())
      if (screen->name() == connector) {
        manufacturer = screen->manufacturer();
        model = screen->model();
      }
    QImage card(1280, 720, QImage::Format_RGB32);
    card.fill(Qt::black);
    QPainter painter(&card);
    QFont font("monospace");
    font.setPixelSize(54);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(card.rect().adjusted(40, 40, -40, -40),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     tr("Display %1\n%2\n%3 %4")
                         .arg(n + 1)
                         .arg(connector, manufacturer, model));
    painter.end();
    const QString path = m_runtime.filePath(QString("identify-%1.png").arg(n));
    if (!card.save(path)) {
      dismissIdentify();
      fail(tr("Cannot create the test pattern."));
      return;
    }
    // Qt's top-level screen hint can be ignored on Wayland. Route these
    // temporary cards through the same connector-aware mpv path as projection.
    auto *process = new QProcess(this);
    m_identify << process;
    process->setStandardOutputFile(QProcess::nullDevice());
    process->setStandardErrorFile(QProcess::nullDevice());
    connect(
        process, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError) {
          dismissIdentify();
          fail(tr("Cannot start mpv. Install the mpv package and try again."));
        });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) { dismissIdentify(); });
    QStringList args{"--no-config",
                     "--terminal=no",
                     "--osc=no",
                     "--osd-level=0",
                     "--audio=no",
                     "--image-display-duration=inf",
                     "--input-default-bindings=no",
                     "--input-vo-keyboard=yes",
                     "--input-conf=" + inputPath,
                     "--cursor-autohide=always",
                     "--screen-name=" + connector,
                     "--fs-screen-name=" + connector};
    if (m_headless)
      args << "--vo=null" << "--ao=null";
    else
      args << "--fullscreen=yes" << "--force-window=immediate"
           << "--wayland-app-id=beam-identify"
           << "--title=Beam display identification";
    args << "--" << path;
    process->start("mpv", args);
  }
  m_identifyTimer.start();
}
void Backend::dismissIdentify() {
  m_identifyTimer.stop();
  const auto processes = m_identify;
  m_identify.clear();
  for (auto *process : processes) {
    disconnect(process, nullptr, this, nullptr);
    if (process->state() != QProcess::NotRunning) {
      process->terminate();
      if (!process->waitForFinished(500)) {
        process->kill();
        process->waitForFinished(500);
      }
    }
    process->deleteLater();
  }
}
void Backend::testPattern() {
  if (!localOutput())
    return;
  if (m_media) {
    fail(tr("Stop playback before showing a test pattern."));
    return;
  }
  if (!ready()) {
    fail(tr("Start projection before showing a test pattern."));
    return;
  }
  QImage image(1280, 720, QImage::Format_RGB32);
  QPainter p(&image);
  const QColor colors[] = {Qt::white,   Qt::yellow, Qt::cyan, Qt::green,
                           Qt::magenta, Qt::red,    Qt::blue, Qt::black};
  for (int i = 0; i < 8; ++i)
    p.fillRect(i * 160, 0, 160, 720, colors[i]);
  p.setPen(QPen(Qt::white, 4));
  p.drawRect(2, 2, 1275, 715);
  p.setPen(QPen(Qt::black, 3));
  p.drawEllipse(QPoint(640, 360), 250, 250);
  p.drawLine(600, 360, 680, 360);
  p.drawLine(640, 320, 640, 400);
  p.end();
  const QString path = m_runtime.filePath("test-pattern.png");
  if (!image.save(path)) {
    fail(tr("Cannot create the test pattern."));
    return;
  }
  m_testing = true;
  command({"loadfile", path, "replace", -1,
           QJsonObject{
               {"vf", m_blackout ? "lavfi=[drawbox=c=black:t=fill]" : ""}}});
  command({"set_property", "pause", false});
  invalidatePreview();
  emit changed();
}
