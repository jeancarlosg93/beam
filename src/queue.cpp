#include "queue.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QSet>
#include <QTimer>
#include <QUuid>
#include <cmath>
QJsonObject QueueItem::json() const {
  return {{"id", id},         {"path", path},   {"start", start},
          {"volume", volume}, {"audio", audio}, {"subtitle", subtitle},
          {"aspect", aspect}};
}
MediaQueue::MediaQueue(QObject *parent) : QObject(parent) {
  connect(
      &m_probe, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
      [this](int code, QProcess::ExitStatus) {
        const auto data =
            QJsonDocument::fromJson(m_probe.readAllStandardOutput()).object();
        const int i = index(m_probing);
        if (i >= 0 && code == 0 && items[i].path == m_probePath) {
          auto &item = items[i];
          bool known = false;
          const double duration =
              data["format"].toObject()["duration"].toString().toDouble(&known);
          item.duration =
              known && std::isfinite(duration) && duration >= 0 ? duration : -1;
          int aid = 0, sid = 0;
          for (const auto &v : data["streams"].toArray()) {
            const auto s = v.toObject();
            const auto type = s["codec_type"].toString();
            if (type != "audio" && type != "subtitle")
              continue;
            const auto tags = s["tags"].toObject();
            item.tracks << QVariantMap{
                {"type", type},
                {"id", QString::number(type == "audio" ? ++aid : ++sid)},
                {"label",
                 QString("%1 · %2 · %3")
                     .arg(type == "audio" ? tr("Audio") : tr("Subtitle"),
                          tags["language"].toString(tr("Unknown language")),
                          tags["title"].toString(s["codec_name"].toString()))}};
          }
        }
        m_probing.clear();
        emit updated();
        probeNext();
      });
  connect(&m_probe, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
              m_probing.clear();
              m_pending.clear();
              emit failed(tr("Cannot start ffprobe. Install ffmpeg to inspect "
                             "media durations and tracks."));
              emit updated();
            }
          });
}
MediaQueue::~MediaQueue() {
  disconnect(&m_probe, nullptr, this, nullptr);
  m_pending.clear();
  if (m_probe.state() != QProcess::NotRunning) {
    m_probe.kill();
    m_probe.waitForFinished(1000);
  }
}
int MediaQueue::index(const QString &id) const {
  for (int i = 0; i < items.size(); ++i)
    if (items[i].id == id)
      return i;
  return -1;
}
QVariantList MediaQueue::list() const {
  QVariantList out;
  for (const auto &i : items) {
    auto v = i.json().toVariantMap();
    v["title"] = QFileInfo(i.path).completeBaseName();
    v["missing"] = !QFileInfo(i.path).isReadable();
    v["duration"] = i.duration;
    v["tracks"] = i.tracks;
    out << v;
  }
  return out;
}
QJsonArray MediaQueue::json() const {
  QJsonArray a;
  for (const auto &i : items)
    a << i.json();
  return a;
}
void MediaQueue::load(const QJsonArray &array) {
  items.clear();
  m_pending.clear();
  QSet<QString> ids;
  for (const auto &v : array) {
    const auto o = v.toObject();
    QueueItem i;
    i.id = o["id"].toString();
    if (i.id.isEmpty() || ids.contains(i.id))
      i.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ids.insert(i.id);
    i.path = o["path"].toString();
    if (i.path.isEmpty())
      continue;
    i.start = qMax(0., o["start"].toDouble());
    i.volume = qBound(-1, o["volume"].toInt(-1), 100);
    i.audio = o["audio"].toString("auto");
    i.subtitle = o["subtitle"].toString("no");
    i.aspect = o["aspect"].toString("preserve");
    items << i;
    m_pending.enqueue(i.id);
  }
  emit updated();
  probeNext();
}
void MediaQueue::add(const QString &path) {
  QueueItem i;
  i.path = path;
  i.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  items << i;
  probe(i.id);
}
void MediaQueue::probe(const QString &id) {
  if (!m_pending.contains(id))
    m_pending.enqueue(id);
  probeNext();
}
void MediaQueue::probeNext() {
  if (!m_probing.isEmpty() || m_pending.isEmpty())
    return;
  m_probing = m_pending.dequeue();
  const int i = index(m_probing);
  if (i < 0) {
    m_probing.clear();
    QTimer::singleShot(0, this, &MediaQueue::probeNext);
    return;
  }
  items[i].tracks.clear();
  m_probePath = items[i].path;
  const int generation = ++m_probeGeneration;
  m_probe.start("ffprobe", {"-v", "error", "-show_format", "-show_streams",
                            "-of", "json", "-i", items[i].path});
  QTimer::singleShot(10000, this, [this, generation] {
    if (m_probeGeneration == generation &&
        m_probe.state() != QProcess::NotRunning)
      m_probe.kill();
  });
}
QString MediaQueue::total() const {
  double seconds = 0;
  int unknown = 0;
  for (const auto &i : items) {
    if (i.duration < 0)
      ++unknown;
    else
      seconds += i.duration;
  }
  const int s = int(seconds);
  const QString time = QString("%1:%2:%3")
                           .arg(s / 3600)
                           .arg(s / 60 % 60, 2, 10, QChar('0'))
                           .arg(s % 60, 2, 10, QChar('0'));
  return unknown ? tr("%1 known · %n unknown", "", unknown).arg(time) : time;
}
