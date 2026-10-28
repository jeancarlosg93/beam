#pragma once
#include "queue.h"
#include <QJsonArray>
#include <QLocalSocket>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
class Backend : public QObject {
  friend class BeamTests;
  Q_OBJECT
  Q_PROPERTY(QVariantList playlist READ playlist NOTIFY playlistChanged)
  Q_PROPERTY(QStringList screens READ screens NOTIFY changed)
  Q_PROPERTY(QStringList screenLabels READ screenLabels NOTIFY changed)
  Q_PROPERTY(QString screen READ screen WRITE setScreen NOTIFY changed)
  Q_PROPERTY(int current READ current NOTIFY changed)
  Q_PROPERTY(int cueIndex READ cueIndex NOTIFY changed)
  Q_PROPERTY(QVariantMap cueItem READ cueItem NOTIFY playlistChanged)
  Q_PROPERTY(QString totalDuration READ totalDuration NOTIFY playlistChanged)
  Q_PROPERTY(QString playlistName READ playlistName NOTIFY changed)
  Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
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
  Q_PROPERTY(bool blackout READ blackout WRITE setBlackout NOTIFY changed)
  Q_PROPERTY(bool blackoutAudio READ blackoutAudio WRITE setBlackoutAudio NOTIFY
                 changed)
  Q_PROPERTY(bool previewEnabled READ previewEnabled WRITE setPreviewEnabled
                 NOTIFY changed)
  Q_PROPERTY(QString previewMode READ previewMode WRITE setPreviewMode NOTIFY changed)
  Q_PROPERTY(QString livePreview READ livePreview NOTIFY previewChanged)
  Q_PROPERTY(QString cuePreview READ cuePreview NOTIFY previewChanged)
  Q_PROPERTY(QString previewMessage READ previewMessage NOTIFY previewChanged)
  Q_PROPERTY(QString outputState READ outputState NOTIFY changed)
  Q_PROPERTY(QVariantList audioDevices READ audioDevices NOTIFY changed)
  Q_PROPERTY(
      QString audioDevice READ audioDevice WRITE setAudioDevice NOTIFY changed)
  Q_PROPERTY(bool audioDeviceAvailable READ audioDeviceAvailable NOTIFY changed)
  Q_PROPERTY(bool testing READ testing NOTIFY changed)
public:
  explicit Backend(const QString &stateFile = {}, bool headless = false,
                   QObject *parent = nullptr);
  ~Backend() override;
  QVariantList playlist() const { return m_queue.list(); }
  int count() const { return m_queue.items.size(); }
  QString mediaPath(int index) const {
    return index >= 0 && index < count() ? m_queue.items[index].path
                                         : QString();
  }
  QString mediaId(int index) const {
    return index >= 0 && index < count() ? m_queue.items[index].id : QString();
  }
  Q_INVOKABLE void cue(int index);
  void pause();
  QStringList screens() const { return m_screens; }
  QStringList screenLabels() const { return m_screenLabels; }
  QString screen() const { return m_screen; }
  int current() const { return m_current; }
  int cueIndex() const { return m_cue; }
  QVariantMap cueItem() const { return playlist().value(m_cue).toMap(); }
  QString totalDuration() const { return m_queue.total(); }
  QString playlistName() const;
  bool dirty() const { return m_dirty; }
  bool projecting() const { return m_process.state() != QProcess::NotRunning; }
  bool playing() const { return m_media && !m_paused; }
  bool mediaActive() const { return m_media; }
  bool ready() const {
    return m_socket.state() == QLocalSocket::ConnectedState;
  }
  double position() const { return m_position; }
  double duration() const { return m_duration; }
  int volume() const { return m_volume; }
  bool muted() const { return m_muted; }
  double speed() const { return m_speed; }
  bool continuous() const { return m_continuous; }
  bool repeat() const { return m_repeat; }
  QString placeholder() const { return m_placeholder; }
  QString error() const { return m_error; }
  bool blackout() const { return m_blackout; }
  bool blackoutAudio() const { return m_blackoutAudio; }
  bool previewEnabled() const { return m_previewEnabled; }
  QString previewMode() const { return m_previewMode; }
  QString livePreview() const { return m_livePreview; }
  QString cuePreview() const { return m_cuePreview; }
  QString previewMessage() const { return m_previewMessage; }
  QString outputState() const;
  QVariantList audioDevices() const { return m_audioDevices; }
  QString audioDevice() const { return m_audioDevice; }
  bool audioDeviceAvailable() const;
  bool testing() const { return m_testing; }
  void setScreen(const QString &value);
  void setVolume(int value);
  void setMuted(bool value);
  void setSpeed(double value);
  void setContinuous(bool value);
  void setRepeat(bool value);
  void setBlackout(bool value);
  void setBlackoutAudio(bool value);
  void setPreviewEnabled(bool value);
  void setPreviewMode(const QString &value);
  void setAudioDevice(const QString &value);
  void setAudioDevices(const QVariantList &devices);
  void updateScreens(const QStringList &names, const QStringList &labels);
  Q_INVOKABLE void refreshAudioDevices();
  Q_INVOKABLE void addFiles(const QList<QUrl> &urls);
  Q_INVOKABLE void chooseFiles(bool placeholder = false);
  Q_INVOKABLE void choosePlaylist(const QString &action);
  Q_INVOKABLE bool savePlaylist(const QString &path, bool m3u = false);
  Q_INVOKABLE bool loadPlaylist(const QString &path, bool discard = false);
  Q_INVOKABLE void setCueSetting(const QString &key, const QVariant &value);
  Q_INVOKABLE void resetCueSettings();
  Q_INVOKABLE void relinkCue(const QUrl &url);
  Q_INVOKABLE void clearPlaceholder();
  Q_INVOKABLE void project();
  Q_INVOKABLE void endProjection();
  Q_INVOKABLE void play(int index = -1, bool paused = false);
  Q_INVOKABLE void playCue() { play(m_cue); }
  Q_INVOKABLE void togglePause();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void seek(double seconds);
  Q_INVOKABLE void move(int from, int to);
  Q_INVOKABLE void remove(int index);
  Q_INVOKABLE void shuffle();
  Q_INVOKABLE void identifyDisplays();
  Q_INVOKABLE void dismissIdentify();
  Q_INVOKABLE void testPattern();
  Q_INVOKABLE void dismissError() {
    m_error.clear();
    emit changed();
  }
  Q_INVOKABLE QString clock(double seconds) const;
  void setPlaceholder(const QUrl &url);
signals:
  void changed();
  void playlistChanged();
  void previewChanged();
  void seeked(double seconds);
private slots:
  void portalResponse(uint response, const QVariantMap &results);

private:
  void command(const QJsonArray &args, const QString &purpose = {});
  void readMessages();
  void standby();
  void save();
  void edited();
  void refreshScreens();
  void fail(const QString &message);
  void applyItem();
  void applyAudio();
  void capturePreview();
  void captureCue();
  void resetCuePreview();
  void invalidatePreview();
  void openPicker(const QString &action);
  MediaQueue m_queue;
  QueueItem m_liveItem;
  QStringList m_screens, m_screenLabels;
  QString m_screen, m_placeholder, m_error, m_stateFile, m_playlistFile,
      m_pickerAction;
  int m_current = -1, m_cue = -1, m_volume = 80;
  double m_position = 0, m_duration = 0, m_speed = 1;
  bool m_media = false, m_paused = true, m_muted = false, m_dirty = false;
  bool m_desiredPaused = true, m_pendingPaused = false;
  bool m_continuous = false, m_repeat = false, m_headless,
       m_pickerPending = false;
  bool m_blackout = false, m_blackoutAudio = true, m_previewEnabled = false,
       m_testing = false;
  QString m_pendingId, m_audioDevice = "auto", m_livePreview, m_cuePreview,
                       m_previewMessage;
  QString m_capturePath, m_cuePath, m_cueToken, m_deviceWarning;
  QString m_previewMode = "low";
  int m_retries = 0, m_requestId = 0, m_captureSequence = 0,
      m_previewGeneration = 0;
  QHash<int, QString> m_requests;
  QVariantList m_audioDevices;
  QList<QProcess *> m_identify;
  QProcess m_process, m_deviceProcess, m_cueProcess;
  QLocalSocket m_socket;
  QTimer m_connectTimer, m_previewTimer, m_cueTimer, m_identifyTimer;
  QTemporaryDir m_runtime;
  QByteArray m_buffer, m_log;
};
