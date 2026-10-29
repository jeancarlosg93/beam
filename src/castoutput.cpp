#include "castoutput.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
CastOutput::CastOutput(QObject *parent) : OutputDriver(parent) {
  m_deadline.setSingleShot(true);
  m_deadline.setInterval(25000);
  connect(&m_deadline, &QTimer::timeout, this, [this] {
    close();
    emit failed(
        tr("Chromecast did not respond. Check the network and try again."));
  });
  connect(&m_process, &QProcess::readyReadStandardOutput, this,
          &CastOutput::read);
  // Library diagnostics never share the JSON protocol stream.
  connect(&m_process, &QProcess::readyReadStandardError, this,
          [this] { m_process.readAllStandardError(); });
  connect(&m_process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
              close();
              emit failed(
                  tr("Cannot start Chromecast support. Run "
                     "bin/setup-chromecast from the Beam source folder."));
            }
          });
  connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int code, QProcess::ExitStatus) {
            m_deadline.stop();
            m_ready = m_active = m_discovering = false;
            m_buffer.clear();
            emit stateChanged();
            if (code && !m_closing && !m_hadError)
              emit failed(tr("Chromecast support stopped. Run "
                             "bin/setup-chromecast and retry."));
          });
}
CastOutput::~CastOutput() { close(); }
bool CastOutput::start() {
  if (m_process.state() != QProcess::NotRunning)
    return true;
  const QString script = m_dir.filePath("chromecast.py");
  if (!QFile::exists(script) && !QFile::copy(":/chromecast.py", script)) {
    emit failed(tr("Cannot prepare Chromecast support."));
    return false;
  }
  QString python = qEnvironmentVariable("BEAM_CAST_PYTHON");
  if (python.isEmpty()) {
    python = QDir::homePath() + "/.local/share/beam/cast-env/bin/python";
    if (!QFile::exists(python))
      python = "python3";
  }
  m_buffer.clear();
  m_hadError = false;
  m_process.start(python, {"-u", script});
  return true;
}
void CastOutput::send(const QJsonObject &request) {
  if (m_process.state() != QProcess::NotRunning)
    m_process.write(QJsonDocument(request).toJson(QJsonDocument::Compact) +
                    '\n');
}
void CastOutput::discover() {
  if (m_active || m_discovering || !start())
    return;
  m_discovering = true;
  m_deadline.start();
  send({{"op", "discover"}});
  emit stateChanged();
}
void CastOutput::open(const QString &id) {
  if (id.isEmpty() || m_active || m_discovering || !start())
    return;
  m_active = true;
  m_deadline.start();
  send({{"op", "connect"}, {"id", id}});
  emit stateChanged();
}
void CastOutput::close() {
  m_closing = true;
  m_deadline.stop();
  if (m_process.state() != QProcess::NotRunning) {
    send({{"op", "disconnect"}});
    m_process.closeWriteChannel();
    if (!m_process.waitForFinished(2500)) {
      m_process.kill();
      m_process.waitForFinished(1000);
    }
  }
  m_ready = m_active = m_discovering = false;
  emit stateChanged();
  m_closing = false;
}
void CastOutput::read() {
  m_buffer += m_process.readAllStandardOutput();
  while (m_buffer.contains('\n')) {
    const int end = m_buffer.indexOf('\n');
    const auto o = QJsonDocument::fromJson(m_buffer.left(end)).object();
    m_buffer.remove(0, end + 1);
    const auto event = o["event"].toString();
    if (event == "devices") {
      m_deadline.stop();
      m_discovering = false;
      emit devicesFound(o["devices"].toVariant().toList());
    } else if (event == "connected") {
      m_deadline.stop();
      m_ready = true;
    } else if (event == "error") {
      m_hadError = true;
      m_deadline.stop();
      m_discovering = false;
      emit failed(o["message"].toString());
    } else if (event == "disconnected") {
      m_ready = m_active = false;
    } else if (event == "status")
      emit status(o);
    emit stateChanged();
  }
}
