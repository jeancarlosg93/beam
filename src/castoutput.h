#pragma once
#include "outputdriver.h"
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
class CastOutput : public OutputDriver {
  Q_OBJECT
public:
  explicit CastOutput(QObject *parent = nullptr);
  ~CastOutput() override;
  bool active() const override { return m_active; }
  bool ready() const override { return m_ready; }
  void discover() override;
  void open(const QString &id) override;
  void close() override;
  void send(const QJsonObject &request) override;
  bool discovering() const { return m_discovering; }

private:
  bool start();
  void read();
  QTemporaryDir m_dir;
  QProcess m_process;
  QTimer m_deadline;
  QByteArray m_buffer;
  bool m_active = false, m_ready = false, m_discovering = false,
       m_closing = false, m_hadError = false;
};
