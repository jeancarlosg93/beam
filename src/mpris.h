#pragma once
#include <QObject>
#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QVariantMap>
#include "backend.h"

class Mpris : public QObject {
    Q_OBJECT
public:
    explicit Mpris(Backend *backend, const QString &service = {}, QObject *parent = nullptr);
    ~Mpris() override;
    bool registered() const { return m_registered; }
    Backend *backend;
    void publish();
signals:
    void raiseRequested();
private:
    QString m_service;
    bool m_registered = false;
    QVariantMap m_last;
};

class MprisRoot : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ schemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ mimeTypes CONSTANT)
public:
    explicit MprisRoot(Mpris *owner) : QDBusAbstractAdaptor(owner), m_owner(owner) {}
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return "Beam"; }
    QString desktopEntry() const { return "beam"; }
    QStringList schemes() const { return {"file"}; }
    QStringList mimeTypes() const { return {"video/mp4", "video/x-matroska", "video/webm", "video/quicktime"}; }
public slots:
    void Raise() { emit m_owner->raiseRequested(); }
    void Quit();
private:
    Mpris *m_owner;
};

class MprisPlayer : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ status)
    Q_PROPERTY(QString LoopStatus READ loop WRITE setLoop)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canNext)
    Q_PROPERTY(bool CanGoPrevious READ canPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)
public:
    explicit MprisPlayer(Mpris *owner) : QDBusAbstractAdaptor(owner), b(owner->backend) {
        connect(b, &Backend::seeked, this, [this](double seconds) { emit Seeked(qlonglong(seconds*1000000)); });
    }
    QString status() const { return b->playing() ? "Playing" : b->mediaActive() ? "Paused" : "Stopped"; }
    QString loop() const { return b->repeat() ? "Playlist" : "None"; }
    void setLoop(const QString &s) { if (s == "Playlist" || s == "None") b->setRepeat(s == "Playlist"); }
    double rate() const { return b->speed(); }
    void setRate(double v) { if (v > 0) b->setSpeed(v); }
    QVariantMap metadata() const;
    QDBusObjectPath trackId() const;
    double volume() const { return b->muted() ? 0 : b->volume()/100.; }
    void setVolume(double v) { b->setMuted(false); b->setVolume(qRound(qBound(0.,v,1.)*100)); }
    qlonglong position() const { return qlonglong(b->position()*1000000); }
    double minimumRate() const { return .25; }
    double maximumRate() const { return 3.; }
    bool canNext() const { return b->count() > 0 && (b->current()+1 < b->count() || b->repeat()); }
    bool canPrevious() const { return b->current() > 0; }
    bool canPlay() const { return b->count() > 0; }
    bool canPause() const { return canPlay(); }
    bool canSeek() const { return b->mediaActive() && b->duration() > 0; }
    bool canControl() const { return true; }
    QVariantMap properties() const;
public slots:
    void Play() { if (!b->playing()) b->togglePause(); }
    void Pause() { if (b->playing()) b->pause(); }
    void PlayPause() { b->togglePause(); }
    void Stop() { b->stop(); }
    void Next();
    void Previous();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &track, qlonglong position);
    void OpenUri(const QString &uri);
signals:
    void Seeked(qlonglong Position);
private:
    void advance(int index);
    Backend *b;
};
