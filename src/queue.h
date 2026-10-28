#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QVariantList>
struct QueueItem {
  QString id, path;
  double duration = -1, start = 0;
  int volume = -1;
  QString audio = "auto", subtitle = "no", aspect = "preserve";
  QVariantList tracks;
  QJsonObject json() const;
};
// Owns playlist data and a single asynchronous probe, independent of audience
// playback.
class MediaQueue : public QObject {
  Q_OBJECT
public:
  explicit MediaQueue(QObject *parent = nullptr);
  ~MediaQueue();
  QList<QueueItem> items;
  QVariantList list() const;
  QJsonArray json() const;
  void load(const QJsonArray &array);
  void add(const QString &path);
  void probe(const QString &id);
  int index(const QString &id) const;
  QString total() const;
signals:
  void updated();
  void failed(const QString &message);

private:
  void probeNext();
  QProcess m_probe;
  QQueue<QString> m_pending;
  QString m_probing, m_probePath;
  int m_probeGeneration = 0;
};
