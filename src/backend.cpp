#include "backend.h"
#include <QGuiApplication>
#include <QScreen>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <algorithm>
#include <cmath>

Backend::Backend(const QString &stateFile, bool headless, QObject *parent)
    : QObject(parent), m_stateFile(stateFile), m_headless(headless) {
    if (m_stateFile.isEmpty())
        m_stateFile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/session.json";
    QFile file(m_stateFile);
    if (file.open(QIODevice::ReadOnly)) {
        const auto o = QJsonDocument::fromJson(file.readAll()).object();
        for (const auto &p : o["files"].toArray()) m_paths << p.toString();
        m_placeholder = o["placeholder"].toString();
        m_screen = o["screen"].toString();
        m_volume = qBound(0, o["volume"].toInt(80), 100);
        m_continuous = o["continuous"].toBool();
        m_repeat = o["repeat"].toBool();
        m_current = m_paths.isEmpty() ? -1 : 0;
    }
    refreshScreens();
    connect(qApp, &QGuiApplication::screenAdded, this, [this] { refreshScreens(); });
    connect(qApp, &QGuiApplication::screenRemoved, this, [this] { refreshScreens(); });
    connect(&m_connectTimer, &QTimer::timeout, this, [this] {
        if (++m_retries > 60) { fail(tr("mpv did not respond. End projection and try again.")); endProjection(); return; }
        m_socket.abort();
        m_socket.connectToServer(m_runtime.path() + "/mpv.sock");
    });
    m_connectTimer.setInterval(100);
    connect(&m_socket, &QLocalSocket::connected, this, [this] {
        m_connectTimer.stop();
        int id = 0;
        for (const QString p : {"pause", "time-pos", "duration", "volume", "mute", "speed"})
            command({"observe_property", ++id, p});
        setVolume(m_volume); setMuted(m_muted); setSpeed(m_speed);
        if (m_pendingPlay >= 0) { const int i = m_pendingPlay; m_pendingPlay = -1; play(i); }
        else standby();
        emit changed();
    });
    connect(&m_socket, &QLocalSocket::readyRead, this, &Backend::readMessages);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_log += m_process.readAllStandardError(); m_log = m_log.right(3000);
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) { m_connectTimer.stop(); fail(tr("Cannot start mpv. Install the mpv package and try again.")); }
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_connectTimer.stop(); m_socket.abort(); m_buffer.clear(); m_media = false;
        m_pendingPlay = -1; m_position = m_duration = 0;
        if (code != 0 || status == QProcess::CrashExit)
            fail(tr("mpv exited unexpectedly: %1").arg(QString::fromUtf8(m_log).trimmed()));
        emit changed();
    });
}
Backend::~Backend() { endProjection(); }
void Backend::fail(const QString &s) { m_error = s; emit changed(); }
QVariantList Backend::playlist() const {
    QVariantList items;
    for (const QString &p : m_paths)
        items << QVariantMap{{"title", QFileInfo(p).completeBaseName()}, {"path", p}, {"missing", !QFileInfo::exists(p)}};
    return items;
}
void Backend::refreshScreens() {
    QStringList names;
    for (auto *s : QGuiApplication::screens()) names << s->name();
    m_screens = names;
    if (!names.contains(m_screen)) {
        if (projecting()) { endProjection(); fail(tr("The output display was disconnected. Choose a display to project again.")); }
        m_screen = names.size() > 1 ? names.last() : names.value(0);
    }
    emit changed();
}
void Backend::save() {
    QDir().mkpath(QFileInfo(m_stateFile).absolutePath());
    QSaveFile f(m_stateFile);
    const QJsonObject o{{"files", QJsonArray::fromStringList(m_paths)}, {"placeholder", m_placeholder},
        {"screen", m_screen}, {"volume", m_volume}, {"continuous", m_continuous}, {"repeat", m_repeat}};
    if (!f.open(QIODevice::WriteOnly) || f.write(QJsonDocument(o).toJson()) < 0 || !f.commit())
        fail(tr("Could not save the playlist session. Check available disk space and permissions."));
}
void Backend::setScreen(const QString &s) {
    if (projecting() || !m_screens.contains(s)) return;
    m_screen = s; save(); emit changed();
}
void Backend::addFiles(const QList<QUrl> &urls) {
    for (const auto &u : urls) {
        const QString p = u.toLocalFile();
        if (p.isEmpty() || !QFileInfo(p).isFile()) { fail(tr("Cannot open %1. Choose a local media file.").arg(u.toDisplayString())); continue; }
        m_paths << QFileInfo(p).absoluteFilePath();
    }
    if (m_current < 0 && !m_paths.isEmpty()) m_current = 0;
    save(); emit playlistChanged(); emit changed();
}
void Backend::command(const QJsonArray &args) {
    if (ready()) m_socket.write(QJsonDocument(QJsonObject{{"command", args}}).toJson(QJsonDocument::Compact) + '\n');
}
void Backend::project() {
    if (projecting()) return;
    if (m_screen.isEmpty() && !m_headless) { fail(tr("No display is available.")); return; }
    m_error.clear(); m_log.clear(); m_retries = 0; m_buffer.clear();
    QStringList args{"--no-config", "--idle=yes", "--keep-open=no", "--terminal=no", "--osc=no", "--osd-level=0",
        "--input-default-bindings=no", "--input-vo-keyboard=no", "--cursor-autohide=always", "--stop-screensaver=yes",
        "--image-display-duration=inf", "--audio-client-name=Beam", "--input-ipc-server=" + m_runtime.path() + "/mpv.sock"};
    if (m_headless) args << "--vo=null" << "--ao=null";
    else args << "--force-window=immediate" << "--fullscreen=yes" << "--screen-name=" + m_screen
              << "--fs-screen-name=" + m_screen << "--wayland-app-id=beam-output" << "--title=Beam output";
    m_process.start("mpv", args);
    m_connectTimer.start(); emit changed();
}
void Backend::endProjection() {
    m_connectTimer.stop(); m_pendingPlay = -1;
    if (m_process.state() != QProcess::NotRunning) {
        command({"quit"}); m_socket.flush();
        if (!m_process.waitForFinished(1500)) { m_process.terminate(); if (!m_process.waitForFinished(500)) { m_process.kill(); m_process.waitForFinished(500); } }
    }
    m_socket.abort(); m_media = false; emit changed();
}
void Backend::play(int i) {
    if (i < 0) i = m_current;
    if (i < 0 || i >= m_paths.size()) return;
    if (!QFileInfo::exists(m_paths[i])) { fail(tr("File is missing: %1. Remove it or add the file again.").arg(m_paths[i])); return; }
    if (!ready()) { m_pendingPlay = i; project(); return; }
    m_current = i; m_media = true; m_paused = false; m_position = m_duration = 0;
    command({"loadfile", m_paths[i], "replace"}); command({"set_property", "pause", false});
    command({"set_property", "speed", m_speed}); emit changed();
}
void Backend::standby() {
    m_media = false; m_paused = true; m_position = m_duration = 0;
    command({"stop"});
    if (!m_placeholder.isEmpty()) {
        if (QFileInfo::exists(m_placeholder)) {
            command({"loadfile", m_placeholder, "replace"}); command({"set_property", "pause", false});
        } else fail(tr("Standby image is missing. Showing black instead."));
    }
    emit changed();
}
void Backend::stop() { m_pendingPlay = -1; standby(); }
void Backend::togglePause() {
    if (!m_media) { play(); return; }
    command({"cycle", "pause"});
}
void Backend::next() {
    if (m_paths.isEmpty()) return;
    if (m_current + 1 < m_paths.size()) play(m_current + 1);
    else if (m_repeat) play(0);
    else stop();
}
void Backend::previous() { if (m_current > 0) play(m_current - 1); else if (m_current == 0) play(0); }
void Backend::seek(double s) { if (m_media) command({"seek", qBound(0., s, m_duration), "absolute+exact"}); }
void Backend::setVolume(int v) { m_volume = qBound(0, v, 100); command({"set_property", "volume", m_volume}); save(); emit changed(); }
void Backend::setMuted(bool v) { m_muted = v; command({"set_property", "mute", v}); emit changed(); }
void Backend::setSpeed(double v) { m_speed = qBound(.25, v, 3.); command({"set_property", "speed", m_speed}); emit changed(); }
void Backend::setContinuous(bool v) { m_continuous = v; save(); emit changed(); }
void Backend::setRepeat(bool v) { m_repeat = v; save(); emit changed(); }
void Backend::move(int from, int to) {
    if (from < 0 || to < 0 || from >= m_paths.size() || to >= m_paths.size()) return;
    m_paths.move(from, to);
    if (m_current == from) m_current = to;
    else if (from < m_current && to >= m_current) --m_current;
    else if (from > m_current && to <= m_current) ++m_current;
    save(); emit playlistChanged(); emit changed();
}
void Backend::remove(int i) {
    if (i < 0 || i >= m_paths.size()) return;
    if (i == m_current) stop();
    m_paths.removeAt(i);
    if (i < m_current) --m_current;
    if (m_current >= m_paths.size()) m_current = m_paths.size()-1;
    save(); emit playlistChanged(); emit changed();
}
void Backend::shuffle() {
    if (m_paths.size() < 2) return;
    // Preserve the exact active occurrence, including duplicate paths.
    QList<int> order; for (int i=0; i<m_paths.size(); ++i) order << i;
    std::shuffle(order.begin(), order.end(), *QRandomGenerator::global());
    QStringList paths; for (int i : order) paths << m_paths[i];
    m_current = order.indexOf(m_current); m_paths = paths;
    save(); emit playlistChanged(); emit changed();
}
QString Backend::clock(double seconds) const {
    const int s = qMax(0, int(seconds));
    return s >= 3600 ? QString("%1:%2:%3").arg(s/3600).arg(s/60%60,2,10,QChar('0')).arg(s%60,2,10,QChar('0'))
                     : QString("%1:%2").arg(s/60).arg(s%60,2,10,QChar('0'));
}
void Backend::setPlaceholder(const QUrl &url) {
    const auto p = url.toLocalFile();
    if (!QFileInfo(p).isFile() || !QImageReader(p).canRead()) { fail(tr("Choose a readable local image file (PNG, JPEG or WebP).")); return; }
    m_placeholder = p; save(); if (ready() && !m_media) standby(); emit changed();
}
void Backend::clearPlaceholder() { m_placeholder.clear(); save(); if (ready() && !m_media) standby(); emit changed(); }
void Backend::readMessages() {
    m_buffer += m_socket.readAll();
    while (m_buffer.contains('\n')) {
        const int end = m_buffer.indexOf('\n');
        const auto o = QJsonDocument::fromJson(m_buffer.left(end)).object(); m_buffer.remove(0, end+1);
        const QString event = o["event"].toString();
        if (event == "property-change") {
            const auto name = o["name"].toString(); const auto v = o["data"];
            if (name == "pause") m_paused = v.toBool();
            if (name == "time-pos" && m_media) m_position = v.toDouble();
            if (name == "duration" && m_media) m_duration = v.toDouble();
            if (name == "volume") m_volume = qBound(0, int(v.toDouble()), 100);
            if (name == "mute") m_muted = v.toBool();
            if (name == "speed") m_speed = v.toDouble(1);
            emit changed();
        } else if (event == "end-file" && m_media) {
            const auto reason = o["reason"].toString();
            if (reason == "eof") { if (m_continuous) next(); else standby(); }
            else if (reason == "error") { standby(); fail(tr("mpv could not play this file: %1").arg(o["file_error"].toString())); }
        } else if (o.contains("error") && o["error"].toString() != "success")
            fail(tr("mpv: %1").arg(o["error"].toString()));
    }
}
void Backend::chooseFiles(bool placeholder) {
    if (m_pickerPending) return;
    m_pickerPending = true; m_pickerPlaceholder = placeholder;
    const QString token = "beam" + QString::number(QRandomGenerator::global()->generate());
    QString sender = QDBusConnection::sessionBus().baseService().mid(1); sender.replace('.', '_');
    const QString path = "/org/freedesktop/portal/desktop/request/" + sender + "/" + token;
    auto bus = QDBusConnection::sessionBus();
    bus.connect("org.freedesktop.portal.Desktop", path, "org.freedesktop.portal.Request", "Response", this, SLOT(portalResponse(uint,QVariantMap)));
    QDBusInterface portal("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.FileChooser", bus);
    QVariantMap options{{"handle_token", token}, {"multiple", !placeholder}, {"modal", false}};
    const auto call = portal.asyncCall("OpenFile", QString(), placeholder ? tr("Choose standby image") : tr("Add media"), options);
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher](QDBusPendingCallWatcher *) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) { m_pickerPending = false; fail(tr("Cannot open the system file picker: %1. You can also drop files into the playlist.").arg(reply.error().message())); }
        watcher->deleteLater();
    });
}
void Backend::portalResponse(uint response, const QVariantMap &results) {
    m_pickerPending = false;
    if (response != 0) return;
    const QStringList uris = qdbus_cast<QStringList>(results.value("uris"));
    QList<QUrl> urls; for (const auto &s : uris) urls << QUrl(s);
    if (m_pickerPlaceholder && !urls.isEmpty()) setPlaceholder(urls.first());
    else addFiles(urls);
}
