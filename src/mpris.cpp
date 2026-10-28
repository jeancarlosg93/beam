#include "mpris.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QFileInfo>
#include <QTimer>

Mpris::Mpris(Backend *b, const QString &service, QObject *parent) : QObject(parent), backend(b) {
    new MprisRoot(this);
    new MprisPlayer(this);
    auto bus = QDBusConnection::sessionBus();
    m_service = service.isEmpty() ? "org.mpris.MediaPlayer2.beam" : service;
    m_registered = bus.registerService(m_service);
    if (!m_registered && service.isEmpty()) {
        m_service += ".instance" + QString::number(QCoreApplication::applicationPid());
        m_registered = bus.registerService(m_service);
    }
    if (m_registered && !bus.registerObject("/org/mpris/MediaPlayer2", this, QDBusConnection::ExportAdaptors)) {
        bus.unregisterService(m_service); m_registered = false;
    }
    if (!m_registered) qWarning("Beam could not register its MPRIS service on the session bus.");
    connect(b, &Backend::changed, this, &Mpris::publish);
    connect(b, &Backend::playlistChanged, this, &Mpris::publish);
    publish();
}
Mpris::~Mpris() {
    if (m_registered) {
        auto bus = QDBusConnection::sessionBus();
        bus.unregisterObject("/org/mpris/MediaPlayer2"); bus.unregisterService(m_service);
    }
}
void Mpris::publish() {
    if (!m_registered) return;
    const auto values = findChild<MprisPlayer *>()->properties();
    QVariantMap changed;
    for (auto it=values.begin(); it!=values.end(); ++it)
        if (!m_last.contains(it.key()) || m_last.value(it.key()) != it.value()) changed[it.key()] = it.value();
    m_last = values;
    // Position is queried directly; publishing every clock tick is prohibited by MPRIS.
    if (changed.isEmpty()) return;
    auto message = QDBusMessage::createSignal("/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "PropertiesChanged");
    message << QString("org.mpris.MediaPlayer2.Player") << changed << QStringList{};
    QDBusConnection::sessionBus().send(message);
}
void MprisRoot::Quit() { QTimer::singleShot(0, qApp, &QCoreApplication::quit); }
QDBusObjectPath MprisPlayer::trackId() const {
    if (b->current()<0) return QDBusObjectPath("/org/mpris/MediaPlayer2/TrackList/NoTrack");
    const auto hash = QCryptographicHash::hash(b->mediaId(b->current()).toUtf8(),QCryptographicHash::Sha256).toHex();
    return QDBusObjectPath("/org/mpris/MediaPlayer2/track/" + QString::fromLatin1(hash));
}
QVariantMap MprisPlayer::metadata() const {
    if (b->current()<0) return {};
    const QString path=b->mediaPath(b->current());
    return {{"mpris:trackid",QVariant::fromValue(trackId())}, {"mpris:length",qlonglong(b->duration()*1000000)},
        {"xesam:title",QFileInfo(path).completeBaseName()}, {"xesam:url",QUrl::fromLocalFile(path).toString()}};
}
QVariantMap MprisPlayer::properties() const {
    return {{"PlaybackStatus",status()}, {"LoopStatus",loop()}, {"Rate",rate()}, {"Metadata",metadata()},
        {"Volume",volume()}, {"CanGoNext",canNext()}, {"CanGoPrevious",canPrevious()},
        {"CanPlay",canPlay()}, {"CanPause",canPause()}, {"CanSeek",canSeek()}};
}
void MprisPlayer::advance(int i) {
    // MPRIS next/previous preserve pause or standby, unlike the explicit cue buttons.
    if (!b->mediaActive()) { b->cue(i); return; }
    const bool paused=!b->playing();
    b->play(i, paused);
}
void MprisPlayer::Next() { if (canNext()) advance((b->current()+1)%b->count()); }
void MprisPlayer::Previous() { if (canPrevious()) advance(b->current()-1); }
void MprisPlayer::Seek(qlonglong offset) {
    if (!canSeek()) return;
    const double target=b->position()+offset/1000000.;
    if (target>b->duration()) { Next(); return; }
    b->seek(target);
}
void MprisPlayer::SetPosition(const QDBusObjectPath &track, qlonglong p) {
    if (!canSeek() || track.path()!=trackId().path() || p<0 || p/1000000.>b->duration()) return;
    b->seek(p/1000000.);
}
void MprisPlayer::OpenUri(const QString &uri) {
    const QUrl url(uri);
    if (!url.isLocalFile()) return;
    const int previousCount=b->count(); b->addFiles({url});
    if (b->count()>previousCount) b->play(b->count()-1);
}
