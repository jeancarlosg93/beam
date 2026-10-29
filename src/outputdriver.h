#pragma once
#include <QJsonObject>
#include <QObject>
#include <QVariantList>

// Protocol-neutral boundary: queue identity and private cues belong to Backend.
// Drivers own discovery, connection, playback and receiver status only.
class OutputDriver : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  virtual bool active() const = 0;
  virtual bool ready() const = 0;
  virtual void discover() = 0;
  virtual void open(const QString &id) = 0;
  virtual void close() = 0;
  virtual void send(const QJsonObject &request) = 0;
signals:
  void devicesFound(const QVariantList &devices);
  void stateChanged();
  void status(const QJsonObject &status);
  void failed(const QString &message);
};
