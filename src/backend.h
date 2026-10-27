#pragma once
#include <QObject>
#include <QUrl>
#include <QJsonArray>
#include <QProcess>
#include <QLocalSocket>
#include <QTimer>
#include <QTemporaryDir>
#include <QVariantList>
#include <QDBusContext>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList playlist READ playlist NOTIFY playlistChanged)
    Q_PROPERTY(QStringList screens READ screens NOTIFY changed)
    Q_PROPERTY(QString screen READ screen WRITE setScreen NOTIFY changed)
    Q_PROPERTY(int current READ current NOTIFY changed)
    Q_PROPERTY(bool projecting READ projecting NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(bool mediaActive READ mediaActive NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(double position READ position NOTIFY changed)
    Q_PROPERTY(double duration READ duration NOTIFY changed)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY changed)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY changed)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY changed)
    Q_PROPERTY(bool continuous READ continuous WRITE setContinuous NOTIFY changed)
    Q_PROPERTY(bool repeat READ repeat WRITE setRepeat NOTIFY changed)
    Q_PROPERTY(QString placeholder READ placeholder NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit Backend(const QString &stateFile = {}, bool headless = false, QObject *parent = nullptr);
    ~Backend() override;
    QVariantList playlist() const;
    QStringList screens() const { return m_screens; }
    QString screen() const { return m_screen; }
    int current() const { return m_current; }
    bool projecting() const { return m_process.state() != QProcess::NotRunning; }
    bool playing() const { return m_media && !m_paused; }
    bool mediaActive() const { return m_media; }
    bool ready() const { return m_socket.state() == QLocalSocket::ConnectedState; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    double speed() const { return m_speed; }
    bool continuous() const { return m_continuous; }
    bool repeat() const { return m_repeat; }
    QString placeholder() const { return m_placeholder; }
    QString error() const { return m_error; }
    void setScreen(const QString &value);
    void setVolume(int value);
    void setMuted(bool value);
    void setSpeed(double value);
    void setContinuous(bool value);
    void setRepeat(bool value);
    Q_INVOKABLE void addFiles(const QList<QUrl> &urls);
    Q_INVOKABLE void chooseFiles(bool placeholder = false);
    Q_INVOKABLE void clearPlaceholder();
    Q_INVOKABLE void project();
    Q_INVOKABLE void endProjection();
    Q_INVOKABLE void play(int index = -1);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void remove(int index);
    Q_INVOKABLE void shuffle();
    Q_INVOKABLE void dismissError() { m_error.clear(); emit changed(); }
    Q_INVOKABLE QString clock(double seconds) const;
    void setPlaceholder(const QUrl &url);
signals:
    void changed();
    void playlistChanged();
private slots:
    void portalResponse(uint response, const QVariantMap &results);
private:
    void command(const QJsonArray &args);
    void readMessages();
    void standby();
    void save();
    void refreshScreens();
    void fail(const QString &message);
    QStringList m_paths, m_screens;
    QString m_screen, m_placeholder, m_error, m_stateFile;
    int m_current = -1, m_volume = 80;
    double m_position = 0, m_duration = 0, m_speed = 1;
    bool m_media = false, m_paused = true, m_muted = false;
    bool m_continuous = false, m_repeat = false, m_headless, m_pickerPlaceholder = false;
    bool m_pickerPending = false;
    int m_pendingPlay = -1, m_retries = 0;
    QProcess m_process;
    QLocalSocket m_socket;
    QTimer m_connectTimer, m_screenTimer;
    QTemporaryDir m_runtime;
    QByteArray m_buffer, m_log;
};
